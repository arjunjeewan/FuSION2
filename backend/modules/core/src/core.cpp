// fusion core: failures, the stop token and the log (section 09, "Errors, failures and reported
// results" and "Stop"; 09.C7, 09.C14).
//
// FuSION 1 has no counterpart to port: it has no try around dispatch, calls exit(1) from inside
// commands and aborts with status 134 on a bad number (Linux/src/main.cc:1148-7945, 4392-4395;
// spec S1.28). Here a failure is a core::Failure with a dotted code, caught once per command by
// the interpreter, and a stop is a request that every step sees through its StopToken.
#include "fusion/core/core.hpp"

#include <atomic>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>

namespace fusion::core {

// ---------------------------------------------------------------- failures

Failure::Failure(std::string code, std::string message, Where where)
    : std::runtime_error(std::move(message)), code_(std::move(code)), where_(std::move(where)) {}

const std::string& Failure::code() const noexcept { return code_; }
const Where& Failure::where() const noexcept { return where_; }

// ---------------------------------------------------------------- stopping
// std::stop_token's semantics (callbacks run once, on the requesting thread; a callback registered
// after the request runs at once; a callback's destructor waits while it runs elsewhere), kept in
// core because libc++ ships std::stop_token only as experimental (11's C++20 subset).

class StopState {
public:
    std::atomic<bool> requested{false};
    std::mutex mutex;
    std::condition_variable done;
    std::map<std::uint64_t, std::function<void()>> callbacks;   // by registration order
    std::uint64_t next_id = 1;
    std::uint64_t running_id = 0;                // the callback request_stop() is running, if any
    std::thread::id running_thread;
};

bool StopToken::stop_requested() const noexcept {
    return state_ && state_->requested.load(std::memory_order_acquire);
}

void StopToken::throw_if_stopped() const {
    if (stop_requested()) throw Stopped();
}

StopSource::StopSource() : state_(std::make_shared<StopState>()) {}

StopToken StopSource::token() const noexcept { return StopToken(state_); }

bool StopSource::request_stop() noexcept {
    StopState& s = *state_;
    std::unique_lock lock(s.mutex);
    if (s.requested.load(std::memory_order_relaxed)) return false;
    s.requested.store(true, std::memory_order_release);
    s.running_thread = std::this_thread::get_id();
    while (!s.callbacks.empty()) {
        auto first = s.callbacks.begin();
        std::function<void()> callback = std::move(first->second);
        s.running_id = first->first;
        s.callbacks.erase(first);
        lock.unlock();
        try {
            callback();
        } catch (...) {
            // A callback that throws must not end the process from a noexcept function; the
            // stop request itself has been made, which is what matters.
        }
        lock.lock();
        s.running_id = 0;
        s.done.notify_all();
    }
    return true;
}

StopCallback::StopCallback(const StopToken& token, std::function<void()> on_stop)
    : state_(token.state_) {
    if (!state_) return;                         // a default token never stops
    std::unique_lock lock(state_->mutex);
    if (state_->requested.load(std::memory_order_relaxed)) {
        lock.unlock();
        on_stop();                               // registered after the request: runs at once
        return;
    }
    id_ = state_->next_id++;
    state_->callbacks.emplace(id_, std::move(on_stop));
}

StopCallback::~StopCallback() {
    if (!state_ || id_ == 0) return;
    std::unique_lock lock(state_->mutex);
    if (state_->callbacks.erase(id_) > 0) return;    // never ran
    if (state_->running_thread == std::this_thread::get_id()) return;   // destroyed by itself
    state_->done.wait(lock, [&] { return state_->running_id != id_; });
}

// ---------------------------------------------------------------- the log

struct Log::Impl {
    bool debug_on = false;
    std::mutex mutex;                            // one whole line per call, whatever the thread
    std::vector<std::shared_ptr<LogSink>> sinks;

    void write(Level level, std::string_view code, std::string_view text) {
        std::lock_guard lock(mutex);
        for (const auto& sink : sinks) sink->write(level, code, text);
    }
};

Log::Log(bool debug_on) : impl_(std::make_unique<Impl>()) { impl_->debug_on = debug_on; }
Log::~Log() = default;

void Log::add_sink(std::shared_ptr<LogSink> sink) {
    std::lock_guard lock(impl_->mutex);
    impl_->sinks.push_back(std::move(sink));
}

bool Log::debug_on() const noexcept { return impl_->debug_on; }

void Log::debug(std::string_view text) {
    if (impl_->debug_on) impl_->write(Level::debug, {}, text);   // S1.29: only under the debug option
}
void Log::info(std::string_view text) { impl_->write(Level::info, {}, text); }
void Log::warning(std::string_view code, std::string_view text) { impl_->write(Level::warning, code, text); }
void Log::error(std::string_view code, std::string_view text) { impl_->write(Level::error, code, text); }

}  // namespace fusion::core
