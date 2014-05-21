#ifndef varint_h
#define varint_h

int GetVarint64(const uint8_t *z, int n, uint64_t *pResult);
int PutVarint64(uint8_t *z, uint64_t x);
int GetVarint32(const uint8_t *z, uint32_t *pResult);
int PutVarint32(uint8_t *p, uint32_t v);
int VarintLen(uint64_t v);

#endif
