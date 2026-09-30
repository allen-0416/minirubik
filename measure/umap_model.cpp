// Reproduce vsrtl::core::AddressSpace::m_data growth: one node per byte.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <sys/resource.h>
static long maxrss_kb() { rusage r; getrusage(RUSAGE_SELF, &r); return r.ru_maxrss; }
int main(int argc, char **argv) {
  uint64_t n = strtoull(argv[1], 0, 0);
  long before = maxrss_kb();
  std::unordered_map<uint64_t, uint8_t> m;
  for (uint64_t a = 0x10100000; a < 0x10100000 + n; a++) m[a] = 1;
  long after = maxrss_kb();
  printf("%llu nodes=%zu buckets=%zu node_sz=%zu dRSS=%ld KB  B/byte=%.2f  buckets_B/byte=%.2f\n",
         (unsigned long long)n, m.size(), m.bucket_count(),
         sizeof(std::__detail::_Hash_node<std::pair<const uint64_t, uint8_t>, false>),
         after - before, (after - before) * 1024.0 / n, m.bucket_count() * 8.0 / n);
}
