#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chrono>
#include <omp.h>
#include "chacha20.h"

void hex_to_raw(const char* src, const int n /*src size*/, uint8_t* dst, const uint8_t* char_to_uint){
  for (int i = omp_get_thread_num(); i < n/2; i = i + omp_get_num_threads()) {
    uint8_t hi = char_to_uint[src[i*2 + 0]];
    uint8_t lo = char_to_uint[src[i*2 + 1]];
    dst[i] = (hi << 4) | lo;
  }
}

void test_keystreams (
    const char *__restrict text_key,
    const char *__restrict text_nonce,
    const char *__restrict text_keystream,
    const uint8_t *__restrict char_to_uint,
    uint8_t *__restrict raw_key,
    uint8_t *__restrict raw_nonce,
    uint8_t *__restrict raw_keystream,
    uint8_t *__restrict result,
    const int text_key_size,
    const int text_nonce_size,
    const int text_keystream_size)

{
  #pragma omp target teams num_teams(1) thread_limit(256)
  {
    #pragma omp parallel 
    {
      hex_to_raw(text_key, text_key_size, raw_key, char_to_uint);
      hex_to_raw(text_nonce, text_nonce_size, raw_nonce, char_to_uint);
      hex_to_raw(text_keystream, text_keystream_size, raw_keystream, char_to_uint);
   
      if (omp_get_thread_num() == 0) {
        Chacha20 chacha(raw_key, raw_nonce);
        chacha.crypt(result, text_keystream_size / 2);
      }
    }
  }
}

int main(int argc, char* argv[])
{
  if (argc != 2) {
    printf("Usage: %s <repeat>\n", argv[0]);
    return 1;
  }
  const int repeat = atoi(argv[1]);

  // Initialize lookup table
  uint8_t char_to_uint[256];
  for (int i = 0; i < 10; i++) char_to_uint[i + '0'] = i;
  for (int i = 0; i < 26; i++) char_to_uint[i + 'a'] = i + 10;
  for (int i = 0; i < 26; i++) char_to_uint[i + 'A'] = i + 10;

  // Sample
  const char *key = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
  const char *nonce = "0001020304050607";
  const char *keystream = "f798a189f195e66982105ffb640bb7757f579da31602fc93ec01ac56f85ac3c134a4547b733b46413042c9440049176905d3be59ea1c53f15916155c2be8241a38008b9a26bc35941e2444177c8ade6689de95264986d95889fb60e84629c9bd9a5acb1cc118be563eb9b3a4a472f82e09a7e778492b562ef7130e88dfe031c79db9d4f7c7a899151b9a475032b63fc385245fe054e3dd5a97a5f576fe064025d3ce042c566ab2c507b138db853e3d6959660996546cc9c4a6eafdc777c040d70eaf46f76dad3979e5c5360c3317166a1c894c94a371876a94df7628fe4eaaf2ccb27d5aaae0ad7ad0f9d4b6ad3b54098746d4524d38407a6deb3ab78fab78c9";

  const int key_len = strlen(key);
  const int nonce_len = strlen(nonce);
  const int keystream_len = strlen(keystream);
  const int result_len = keystream_len / 2;

  uint8_t *raw_key = (uint8_t*) malloc (key_len/2);
  uint8_t *raw_nonce = (uint8_t*) malloc (nonce_len/2);
  uint8_t *raw_keystream = (uint8_t*) malloc (result_len);
  uint8_t *result = (uint8_t*) malloc (result_len);

  #pragma omp target data map(to: char_to_uint[0:256], \
                                  key[0:key_len], \
                                  nonce[0:nonce_len], \
                                  keystream[0:keystream_len]) \
                          map(alloc: raw_key[0:key_len/2], \
                                     raw_nonce[0:nonce_len/2]) \
                          map(from: raw_keystream[0:result_len], \
                                    result[0:result_len])
  {
    auto start = std::chrono::steady_clock::now();

    for (int i = 0; i < repeat; i++) {
      #pragma omp target teams distribute parallel for 
      for (int i = 0; i < result_len; i++)
        result[i] = 0;

      test_keystreams(
        key, nonce, keystream, char_to_uint, 
        raw_key, raw_nonce, raw_keystream, result,
        key_len, nonce_len, keystream_len);
    }

    auto end = std::chrono::steady_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
    printf("Average execution time of kernels: %f (us)\n", (time * 1e-3f) / repeat);
  }

  int error = memcmp(result, raw_keystream, result_len);
  printf("%s\n", error ? "FAIL" : "PASS");

  free(result);
  free(raw_keystream);
  free(raw_key);
  free(raw_nonce);

  return 0;
}
