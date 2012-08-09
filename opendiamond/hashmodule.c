//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

// Note - The x86 and x64 versions do _not_ produce the same results, as the
// algorithms are optimized for their respective platforms. You can still
// compile and run any of them on any platform, but your performance with the
// non-native version will be less than optimal.

// Conversion to a Python module was done by Benjamin Gilbert, and is also
// placed in the public domain.

#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>

#define PYTHON_UNLOCK_THRESHOLD (1 << 20)  /* 0.4 ms on a 2.66 GHz Core 2 */

static inline uint32_t rotl32 ( uint32_t x, int8_t r )
{
  return (x << r) | (x >> (32 - r));
}

static inline uint64_t rotl64 ( uint64_t x, int8_t r )
{
  return (x << r) | (x >> (64 - r));
}

#define	ROTL32(x,y)	rotl32(x,y)
#define ROTL64(x,y)	rotl64(x,y)

#define BIG_CONSTANT(x) (x##LLU)

// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here
static inline uint64_t getblock ( const uint64_t * p, int i )
{
  return p[i];
}

// Finalization mix - force all bits of a hash block to avalanche
static inline uint64_t fmix ( uint64_t k )
{
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xff51afd7ed558ccd);
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
  k ^= k >> 33;

  return k;
}

static void MurmurHash3_x64_128 ( const void * key, const int len,
                                  const uint32_t seed, void * out )
{
  const uint8_t * data = (const uint8_t*)key;
  const int nblocks = len / 16;

  uint64_t h1 = seed;
  uint64_t h2 = seed;

  const uint64_t c1 = BIG_CONSTANT(0x87c37b91114253d5);
  const uint64_t c2 = BIG_CONSTANT(0x4cf5ad432745937f);

  //----------
  // body

  const uint64_t * blocks = (const uint64_t *)(data);
  int i;

  for(i = 0; i < nblocks; i++)
  {
    uint64_t k1 = getblock(blocks,i*2+0);
    uint64_t k2 = getblock(blocks,i*2+1);

    k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;

    h1 = ROTL64(h1,27); h1 += h2; h1 = h1*5+0x52dce729;

    k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

    h2 = ROTL64(h2,31); h2 += h1; h2 = h2*5+0x38495ab5;
  }

  //----------
  // tail

  const uint8_t * tail = (const uint8_t*)(data + nblocks*16);

  uint64_t k1 = 0;
  uint64_t k2 = 0;

  switch(len & 15)
  {
  case 15: k2 ^= ((uint64_t)(tail[14])) << 48;
  case 14: k2 ^= ((uint64_t)(tail[13])) << 40;
  case 13: k2 ^= ((uint64_t)(tail[12])) << 32;
  case 12: k2 ^= ((uint64_t)(tail[11])) << 24;
  case 11: k2 ^= ((uint64_t)(tail[10])) << 16;
  case 10: k2 ^= ((uint64_t)(tail[ 9])) << 8;
  case  9: k2 ^= ((uint64_t)(tail[ 8])) << 0;
           k2 *= c2; k2  = ROTL64(k2,33); k2 *= c1; h2 ^= k2;

  case  8: k1 ^= ((uint64_t)(tail[ 7])) << 56;
  case  7: k1 ^= ((uint64_t)(tail[ 6])) << 48;
  case  6: k1 ^= ((uint64_t)(tail[ 5])) << 40;
  case  5: k1 ^= ((uint64_t)(tail[ 4])) << 32;
  case  4: k1 ^= ((uint64_t)(tail[ 3])) << 24;
  case  3: k1 ^= ((uint64_t)(tail[ 2])) << 16;
  case  2: k1 ^= ((uint64_t)(tail[ 1])) << 8;
  case  1: k1 ^= ((uint64_t)(tail[ 0])) << 0;
           k1 *= c1; k1  = ROTL64(k1,31); k1 *= c2; h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= len; h2 ^= len;

  h1 += h2;
  h2 += h1;

  h1 = fmix(h1);
  h2 = fmix(h2);

  h1 += h2;
  h2 += h1;

  ((uint64_t*)out)[0] = h1;
  ((uint64_t*)out)[1] = h2;
}

static int Test_MurmurHash3_x64_128 ( void )
{
  const int hashbytes = 128 / 8;
  const uint32_t expected = 0x6384BA69;

  uint8_t key[256];
  uint8_t hashes[hashbytes * 256];
  uint8_t final[hashbytes];
  int i;

  memset(key, 0, sizeof(key));
  memset(hashes, 0, sizeof(hashes));
  memset(final, 0, sizeof(final));

  // Hash keys of the form {0}, {0,1}, {0,1,2}... up to N=255,using 256-N as
  // the seed

  for(i = 0; i < 256; i++)
  {
    key[i] = (uint8_t)i;

    MurmurHash3_x64_128(key,i,256-i,&hashes[i*hashbytes]);
  }

  // Then hash the result array

  MurmurHash3_x64_128(hashes,hashbytes*256,0,final);

  // The first four bytes of that hash, interpreted as a little-endian integer, is our
  // verification value

  uint32_t verification = (final[0] << 0) | (final[1] << 8) | (final[2] << 16) | (final[3] << 24);

  return verification == expected;
}

static inline char nibble_to_char(int v)
{
  if (v < 10) {
    return v + '0';
  } else {
    return v - 10 + 'a';
  }
}

static void buf_to_hex(char *out, const unsigned char *in, int in_len)
{
  int i;

  for (i = 0; i < in_len; i++) {
    out[2 * i] = nibble_to_char(in[i] >> 4);
    out[2 * i + 1] = nibble_to_char(in[i] & 0xf);
  }
}

static PyObject *do_murmur3_x64_128(PyObject *self, PyObject *args)
{
  Py_buffer view;
  PY_LONG_LONG seed = 0;
  PyThreadState *save = NULL;
  unsigned char hash[16];
  char hashstr[2 * sizeof(hash)];

  if (!PyArg_ParseTuple(args, "s*|L", &view, &seed)) {
    return NULL;
  }

  if (!PyBuffer_IsContiguous(&view, 'C') ||
      (view.format != NULL && strcmp(view.format, "B"))) {
    PyBuffer_Release(&view);
    PyErr_SetString(PyExc_BufferError, "Invalid buffer format");
    return NULL;
  }

  if (view.len < 0 || view.len > INT_MAX) {
    PyBuffer_Release(&view);
    PyErr_SetString(PyExc_ValueError, "Argument too large");
    return NULL;
  }

  if (seed < 0 || seed > UINT32_MAX) {
    PyBuffer_Release(&view);
    PyErr_SetString(PyExc_ValueError, "Seed out of range");
    return NULL;
  }

  if (view.len >= PYTHON_UNLOCK_THRESHOLD) {
    save = PyEval_SaveThread();
  }

  MurmurHash3_x64_128(view.buf, view.len, seed, hash);
  buf_to_hex(hashstr, hash, sizeof(hash));

  if (view.len >= PYTHON_UNLOCK_THRESHOLD) {
    PyEval_RestoreThread(save);
  }

  PyBuffer_Release(&view);

  return Py_BuildValue("s#", hashstr, sizeof(hashstr));
}

static PyMethodDef HashMethods[] = {
  {"murmur3_x64_128", do_murmur3_x64_128, METH_VARARGS,
      "murmur3_x64_128(data, seed=0) -> string\n\n"
      "Returns a 128-bit MurmurHash3-x64 non-cryptographic hash as a hex\n"
      "string."},
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC inithash(void)
{
  if (!Test_MurmurHash3_x64_128()) {
    PyErr_SetString(PyExc_ImportError, "Murmur hash self-test failed");
    return;
  }

  Py_InitModule("hash", HashMethods);
}
