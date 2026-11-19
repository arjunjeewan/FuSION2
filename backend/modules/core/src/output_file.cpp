// fusion core: OutputFile, the one way the backend writes a file (section 09, "The run directory",
// Rules; cross-section items 1.28, 2A.6 and 2B.3).
//
// FuSION 1 wrote every output straight under its final name with std::ofstream, truncating it
// first, so a failure or a kill left a half-written file under the real name and a failed step
// could truncate an earlier output (spec S3.10; e.g. cudf's lists, Linux/src/main.cc:2650-2699).
// Here a file is written as "<name>.tmp", hashed in the same pass, and renamed when committed;
// an uncommitted file is removed.
#include <algorithm>
#include <array>
#include <atomic>
#include <fstream>
#include <streambuf>
#include <system_error>
#include <thread>

#include "fusion/core/core.hpp"

#if defined(_WIN32)
#include <process.h>
#define FUSION_GETPID _getpid
#else
#include <unistd.h>
#define FUSION_GETPID getpid
#endif

namespace fusion::core {

namespace {

namespace fs = std::filesystem;

// Windows refuses to replace a file another process holds open without delete sharing, and virus
// scanners hold new files briefly: retry ten times, 50 ms apart (1.28). Elsewhere a rename that
// fails will not succeed later.
#if defined(_WIN32)
constexpr int rename_attempts = 10;
#else
constexpr int rename_attempts = 1;
#endif

[[noreturn]] void throw_write(const fs::path& file, const std::string& what) {
    throw Failure("io.write", "cannot write " + to_utf8(file) + ": " + what, Where{to_utf8(file), 0, {}});
}

void rename_into_place(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    for (int attempt = 1; attempt <= rename_attempts; ++attempt) {
        fs::rename(from, to, ec);
        if (!ec) return;
        if (attempt < rename_attempts) std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    throw_write(to, ec.message());
}

// A file stream buffer that feeds every byte it writes to SHA-256 as well, so a committed output
// is hashed in the same pass as the write (09, Algorithms and cost).
class HashingFileBuf final : public std::streambuf {
public:
    bool open(const fs::path& path) {
        if (!file_.open(path, std::ios::out | std::ios::binary | std::ios::trunc)) return false;
        setp(buffer_.data(), buffer_.data() + buffer_.size());
        return true;
    }
    // Writes what is buffered and closes the file; false if any write failed (a full disk).
    bool close() {
        const bool flushed = drain() && file_.pubsync() == 0;
        const bool closed = file_.close() != nullptr;
        return flushed && closed && !failed_;
    }
    Sha256 digest() { return hasher_.finish(); }

protected:
    int_type overflow(int_type ch) override {
        if (!drain()) return traits_type::eof();
        if (!traits_type::eq_int_type(ch, traits_type::eof())) {
            *pptr() = traits_type::to_char_type(ch);
            pbump(1);
        }
        return traits_type::not_eof(ch);
    }
    std::streamsize xsputn(const char* data, std::streamsize count) override {
        if (count < static_cast<std::streamsize>(buffer_.size())) return std::streambuf::xsputn(data, count);
        if (!drain()) return 0;                  // a large block goes straight to the file
        hasher_.update(std::string_view(data, static_cast<std::size_t>(count)));
        if (file_.sputn(data, count) != count) {
            failed_ = true;
            return 0;
        }
        return count;
    }
    int sync() override { return drain() && file_.pubsync() == 0 ? 0 : -1; }

private:
    bool drain() {
        const std::streamsize pending = pptr() - pbase();
        if (pending > 0) {
            hasher_.update(std::string_view(pbase(), static_cast<std::size_t>(pending)));
            if (file_.sputn(pbase(), pending) != pending) failed_ = true;
            setp(buffer_.data(), buffer_.data() + buffer_.size());
        }
        return !failed_;
    }

    std::filebuf file_;
    Sha256Builder hasher_;
    std::array<char, 1 << 16> buffer_{};
    bool failed_ = false;
};

fs::path with_suffix(const fs::path& path, std::string_view suffix) {
    fs::path out = path;
    out += path_from_utf8(suffix);
    return out;
}

std::atomic<std::uint64_t> copy_counter{0};

}  // namespace

struct OutputFile::Impl {
    fs::path final_path;
    fs::path temp_path;
    OnCommit on_commit;
    std::vector<fs::path> also;
    std::unique_ptr<HashingFileBuf> buffer;      // opened by the first out()
    std::unique_ptr<std::ostream> stream;
    bool committed = false;

    ~Impl() {
        if (committed) return;
        if (buffer) buffer->close();
        std::error_code ignored;
        fs::remove(temp_path, ignored);          // the only files FuSION removes: its own *.tmp (09)
    }
};

OutputFile::OutputFile(std::filesystem::path final_path, OnCommit on_commit) : impl_(std::make_unique<Impl>()) {
    impl_->final_path = std::move(final_path);
    impl_->temp_path = with_suffix(impl_->final_path, ".tmp");
    impl_->on_commit = std::move(on_commit);
    // Modules name files such as "in/case.json" inside their step directory (06), so the
    // directories on the way are made here.
    if (const fs::path parent = impl_->final_path.parent_path(); !parent.empty()) {
        std::error_code ec;
        fs::create_directories(parent, ec);
        if (ec) throw_write(impl_->final_path, ec.message());
    }
}

OutputFile::OutputFile(OutputFile&&) noexcept = default;
OutputFile& OutputFile::operator=(OutputFile&&) noexcept = default;
OutputFile::~OutputFile() = default;

std::ostream& OutputFile::out() {
    Impl& s = *impl_;
    if (s.committed) throw Failure("internal", "OutputFile::out after commit: " + to_utf8(s.final_path));
    if (!s.stream) {
        auto buffer = std::make_unique<HashingFileBuf>();
        if (!buffer->open(s.temp_path)) throw_write(s.temp_path, "cannot create the file");
        s.buffer = std::move(buffer);
        s.stream = std::make_unique<std::ostream>(s.buffer.get());
    }
    return *s.stream;
}

void OutputFile::also_at(const std::filesystem::path& absolute) {
    if (!absolute.is_absolute())
        throw Failure("internal", "OutputFile::also_at needs an absolute path: " + to_utf8(absolute));
    impl_->also.push_back(absolute);
}

void OutputFile::commit() {
    Impl& s = *impl_;
    if (s.committed) throw Failure("internal", "OutputFile committed twice: " + to_utf8(s.final_path));

    Sha256 digest;
    if (s.buffer) {
        s.stream->flush();
        const bool stream_ok = !s.stream->bad();
        if (!s.buffer->close() || !stream_ok) throw_write(s.final_path, "the write failed (is the disk full?)");
        digest = s.buffer->digest();
    } else {
        // Nothing went through out(): either another program wrote temp_path() (gnuplot, 07), or
        // the output is empty and its file is made here.
        if (!fs::exists(s.temp_path)) {
            std::ofstream empty(s.temp_path, std::ios::binary);
            if (!empty) throw_write(s.temp_path, "cannot create the file");
        }
        digest = Sha256::of_file(s.temp_path);
    }
    rename_into_place(s.temp_path, s.final_path);
    s.committed = true;
    if (s.on_commit) s.on_commit(s.final_path, digest);

    // 2B.3: an explicit absolute path gets a copy, written under a temporary name of its own, so
    // two runs copying to one path never mix their bytes; the later rename wins.
    for (const fs::path& target : s.also) {
        const auto n = copy_counter.fetch_add(1);
        const fs::path temp =
            with_suffix(target, "." + std::to_string(FUSION_GETPID()) + "-" + std::to_string(n) + ".tmp");
        std::error_code ec;
        fs::copy_file(s.final_path, temp, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            std::error_code ignored;
            fs::remove(temp, ignored);
            throw_write(target, ec.message());
        }
        try {
            rename_into_place(temp, target);
        } catch (...) {
            std::error_code ignored;
            fs::remove(temp, ignored);
            throw;
        }
        if (s.on_commit) s.on_commit(target, digest);
    }
}

const std::filesystem::path& OutputFile::path() const noexcept { return impl_->final_path; }
const std::filesystem::path& OutputFile::temp_path() const noexcept { return impl_->temp_path; }

}  // namespace fusion::core
