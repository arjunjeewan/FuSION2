// fusion network: the run's binding table and bindings.tsv (02.C13; S2.03, S4.02, S4.15, S3.12;
// cross-section item 1.22).
#include <array>
#include <deque>
#include <map>
#include <ostream>

#include "detail.hpp"

namespace fusion::network {

struct BindingTable::Impl {
  std::shared_ptr<const Network> network;
  Maps maps;
  std::deque<BindingRow> rows;                               // stable references; first binding first
  std::map<std::pair<std::string, std::string>, std::size_t> index;   // (role, canonical id) -> row
};

BindingTable::BindingTable(std::shared_ptr<const Network> network, const Maps& maps)
    : impl_(std::make_unique<Impl>()) {
  if (!network) throw core::Failure("internal", "a binding table needs a network");
  impl_->network = std::move(network);
  impl_->maps = maps;
}
BindingTable::BindingTable(BindingTable&&) noexcept = default;
BindingTable& BindingTable::operator=(BindingTable&&) noexcept = default;
BindingTable::~BindingTable() = default;

// S2.03's lookup, made explicit: FuSION 1 resolved an id through two insert-only maps, so the first
// claim won silently (Linux/src/GraphManagerNew.cc:797-808, 876-887). Here each (role, id) is resolved
// once, against the run's network (S4.15), with the id rule applied first (02.C3), and the result
// says whether the node is the id's own, a family, or one of several holders.
const Binding& BindingTable::bind(std::string_view role, std::string_view given, std::uint32_t line) {
  Impl& I = *impl_;
  std::string canonical = I.maps.ids.apply(given);
  auto key = std::make_pair(std::string(role), canonical);
  if (const auto it = I.index.find(key); it != I.index.end()) return *I.rows[it->second].binding;
  const Network& net = *I.network;
  Binding b;
  b.given = std::string(given);
  b.canonical = canonical;
  const auto id = canonical.empty() ? std::nullopt : net.interner().find_id(canonical);
  b.id = id ? *id : absent_id;
  const auto holders = id ? net.ids().holders(*id) : std::span<const NodeIdx>();
  b.holders = static_cast<std::uint32_t>(holders.size());
  if (holders.empty()) {
    b.status = BindStatus::absent;
  } else {
    b.node = holders.front();                                // the first claim (S2.03)
    if (holders.size() > 1) b.status = BindStatus::ambiguous;
    else b.status = net.members(b.node).size() == 1 ? BindStatus::own : BindStatus::family;
  }
  I.rows.push_back(BindingRow{std::string(role), std::string(given), std::move(b), line, {}});
  I.index.emplace(std::move(key), I.rows.size() - 1);
  return *I.rows.back().binding;
}

void BindingTable::add_row(BindingRow row) { impl_->rows.push_back(std::move(row)); }

BindingTable::Counts BindingTable::counts(std::string_view role) const {
  Counts c;
  for (const auto& row : impl_->rows) {
    if (row.role != role || !row.binding) continue;
    ++c.ids;
    switch (row.binding->status) {
      case BindStatus::own: ++c.own; break;
      case BindStatus::family: ++c.family; break;
      case BindStatus::ambiguous: ++c.ambiguous; break;
      case BindStatus::absent: ++c.absent; break;
    }
  }
  return c;
}

namespace {
std::string field(std::string_view text) {
  std::string out(text);
  for (char& c : out)
    if (c == '\t' || c == '\n' || c == '\r') c = ' ';
  return out;
}
std::string_view status_name(BindStatus s) {
  switch (s) {
    case BindStatus::own: return "own";
    case BindStatus::family: return "family";
    case BindStatus::ambiguous: return "ambiguous";
    case BindStatus::absent: return "absent";
  }
  return "absent";
}
}  // namespace

// bindings.tsv (02.C13): role, given, id, status, node, label, size, members, also_in, line, note.
void BindingTable::write(std::ostream& out) const {
  const Network& net = *impl_->network;
  const Maps& maps = impl_->maps;
  out << "role\tgiven\tid\tstatus\tnode\tlabel\tsize\tmembers\talso_in\tline\tnote\n";
  for (const auto& row : impl_->rows) {
    std::array<std::string, 9> cols;
    cols[0] = field(row.role);
    cols[1] = field(row.given);
    if (row.binding) {
      const Binding& b = *row.binding;
      cols[2] = field(b.canonical);
      cols[3] = std::string(status_name(b.status));
      if (b.status != BindStatus::absent) {
        cols[4] = field(net.node_key(b.node).value);
        cols[5] = field(net.label(b.node, maps));
        const auto members = net.members(b.node);
        cols[6] = std::to_string(members.size());
        std::vector<std::string> labels;
        for (GeneId m : members) labels.emplace_back(maps.symbols.label_or_id(net.interner().str(m)));
        cols[7] = field(detail::join(labels, ","));
        std::vector<std::string> others;
        for (NodeIdx h : net.ids().holders(b.id))
          if (h != b.node) others.push_back(net.node_key(h).value);
        cols[8] = field(detail::join(others, ","));
      }
    }
    for (const auto& c : cols) out << c << '\t';
    out << (row.line ? std::to_string(row.line) : std::string()) << '\t' << field(row.note) << '\n';
  }
}

}  // namespace fusion::network
