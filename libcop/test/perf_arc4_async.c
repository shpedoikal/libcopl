/* 
 
Licensed Materials - Property of IBM
   Restricted Materials of IBM"
 
   Copyright 2008,2009
   Chris J Arges
   Everton Constantino
   Tom Gall
   IBM Corp, All Rights Reserved
 
   US Government Users Restricted Rights - Use Duplication or
   disclosure restricted by GSA ADP Schedule Contract with IBM
   Corp
 
 */

/*
January 2010

Testcase developed by Anil Krishna, IBM Corp., based on several example 
testcases written by Chris Arges and team.

This is a testcase to measure performance of the crypto accelerator hardware.

This code tries to stress test the crypto hardware with many requests of a 
given size and for a given cryptographic algorithm. 

This test can then be used with wrapper code that measures throughput and 
latency of the crypto unit.
*/

#include <stdio.h>
#include <libcop.h>
#include <openssl/bn.h>
#include <pthread.h>
#include <semaphore.h>

sem_t central_mutex;
int TYPE;
int PKT_SIZE_BYTES;
int NUM_THREADS;
int NUM_TESTS_PER_THREAD;
int KEY_SIZE_BYTES = 16;

typedef struct {
  int id;
  int fail;
  int total;
} parameters;

int generate_test_data(unsigned char *data, unsigned char *key) 
{
	BIGNUM *BN_key = BN_new();
	BIGNUM *BN_data = BN_new();
	BN_CTX *ctx = BN_CTX_new();
	BN_pseudo_rand(BN_key, KEY_SIZE_BYTES*8, -1, 0);
	BN_pseudo_rand(BN_data, PKT_SIZE_BYTES*8, -1, 0);
	BN_bn2bin(BN_key, key);
	BN_bn2bin(BN_data, data);
	BN_CTX_free(ctx);
}

#define TEST(type){\
	for (i = 0; i < NUM_TESTS_PER_THREAD; i++) {\
	  generate_test_data(indata, key);\
	  if(type(key, indata_len, indata, outdata, NULL, NULL, NULL, COP_FLAG_ONESHOT, ASYNC_INTERRUPT, &crb[i])){\
	    printf(": call failed in thread %d\n", p->id);\
	    p->fail++;\
	  }\
	  p->total++;\
	}\
	for (i = 0; i < NUM_TESTS_PER_THREAD; i++) {\
	  csb[i]=cop_get_csbaddr(crb[i]);\
	  while(csb[i]->valid_bit == 0);\
	  memcpy(outdata, (uint64_t)(crb[i]->ptrTD), indata_len);\
	  cop_give_back_crb(cop_cc_pool(), crb[i], COP_SYM_CRYPTO);\
	}\
}

void work(void *args){
	parameters *p = (parameters*) args;
	printf("Started Thread %03x: \n", p->id);
        int i = 0;

	unsigned char* key;
	key=(char*)malloc(KEY_SIZE_BYTES);
	unsigned char* indata;
	indata=(char*)malloc(PKT_SIZE_BYTES);
	unsigned char* outdata;
	outdata=(char*)malloc(PKT_SIZE_BYTES);

	int indata_len = PKT_SIZE_BYTES;

	sem_wait(&central_mutex);
 
        struct cop_crb *crb[NUM_TESTS_PER_THREAD];
	struct cop_csb *csb[NUM_TESTS_PER_THREAD];

	switch(TYPE){
	  case 1: TEST(cop_rc4_40_async); break;
	  case 2: TEST(cop_rc4_64_async); break;
	  case 3: TEST(cop_rc4_128_async); break;
	  default: printf("Incorrect TYPE\n"); break;
	}
}

int main(int argc, char **argv)
{
	if(argc != 5){
	  printf("Usage: %s   TYPE(1=arc4_40, 2=arc4_64, 3=arc4_128)   PKT_SIZE_BYTES   NUM_THREADS   NUM_TESTS_PER_THREAD\n", argv[0]);
	  return 0;
	} 
	else{
	  TYPE =  atoi(argv[1]);
	  PKT_SIZE_BYTES =  atoi(argv[2]);
	  NUM_THREADS =  atoi(argv[3]);
	  NUM_TESTS_PER_THREAD =  atoi(argv[4]);
	  if(TYPE!=1 && TYPE!=2 && TYPE!=3){
	    printf("TYPE can only be 1, 2 or 3\n");
	    return(0);
	  }
	}

	int i = 0;
	int failTh = 0;
	parameters* params = NULL;
	params = (parameters*)malloc(NUM_THREADS * sizeof(parameters));
	for(i=0; i<NUM_THREADS; i++){
	  params[i].id = i;
	  params[i].fail = 0;
	  params[i].total = 0;
	}

	pthread_t *threads = NULL;
	threads = (pthread_t *)malloc(NUM_THREADS * sizeof(pthread_t*));

	//create all the threads; they initialize and wait on a semaphore
	for(i=0;i<NUM_THREADS;i++){
	  if(pthread_create(&threads[i], NULL, (void*)&work, &params[i])){
	    failTh++;
	  }
	}

	//green flag for each of the threads to go
	for(i=0;i<NUM_THREADS;i++){
	  sem_post(&central_mutex);
	}

	//wait for all threads to finish
	for(i=0;i<NUM_THREADS;i++){
	  if(pthread_join(threads[i], NULL)){
	    failTh++;
	  }
	}

	//report stats on each thread
	for(i=0;i<NUM_THREADS;i++){
	  printf("=== %s: Thread %d: %d/%d failures ===\n", argv[0], i, params[i].fail, params[i].total);
	}

	return failTh;
}
