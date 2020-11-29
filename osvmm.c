//
//  memmgr.c
//  memmgr
//
//  Created by William McCarthy on 17/11/20.
//  Copyright © 2020 William McCarthy. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define ARGC_ERROR 1
#define FILE_ERROR 2
#define BUFLEN 256
#define FRAME_SIZE  256

int pageCount[5], pageCount2[5]; // page fault count
int tlbCount[5], tlbCount2[5]; 
int count[5], count2[5]; 

char mem[65536];
char mem_fifo[32768]; 
int queue[128];
int head=0, tail=0;
int tlb[16][2]; int tlbEntry=0;
int table[256];
int FrameNow=0;
FILE* osos;

unsigned getpage(unsigned x) { return (0xff00 & x) >> 8; }

unsigned getoffset(unsigned x) { return (0xff & x); }

void getpageOffset(unsigned x) {
  unsigned  page   = getpage(x);
  unsigned  offset = getoffset(x);
  printf("x is: %u, page: %u, offset: %u, address: %u, paddress: %u\n", x, page, offset,
         (page << 8) | getoffset(x), page * 256 + offset);
}

int tlbContains(unsigned x){
  for (int i=0; i<16; i++){
    if (tlb[i][0] == x) { return i; }
    return -1;
  }
}

void tlbUpdate(unsigned page){
  tlb[tlbEntry][0] = page;
  tlb[tlbEntry][1] = table[page];
  tlbEntry = (tlbEntry+1) % 16; // round-robin
}

unsigned getframe(unsigned logic_add, unsigned page,
         int *page_fault_count, int *tlb_hit_count){
        
  int tlb_index = tlbContains(page);
  if (tlb_index != -1) {
    (*tlb_hit_count)++;
    return tlb[tlb_index][1];
  }

  if (table[page] != -1) { 
    tlbUpdate(page);
    return table[page];
  }

  
  int offset = (logic_add / FRAME_SIZE) * FRAME_SIZE;
  fseek(osos, offset, 0);


  table[page] = FrameNow;
  FrameNow = (FrameNow+1) % 256;
  (*page_fault_count)++;  
  fread(&main_mem[table[page]*FRAME_SIZE],sizeof(char),256,osos);
  tlbUpdate(page);
  return table[page];
}

int get_available_frame(unsigned page){

  if (head==0 && tail==0 && queue[head]==-1){
    ++tail;
    queue[head]=page;
    return head;
  }


  if (queue[tail]==-1){
    queue[tail]=page;
    int val = tail;
    tail=(tail+1)%128;
    return val;
  }


  if (head==tail && queue[tail]!=-1){
    queue[head]=page;
    int val=head;
    head=(head+1)%128;
    tail=(tail+1)%128;
    return val;
  }
}

unsigned getframe_fifo(unsigned logic_add, unsigned page,
         int *page_fault_count, int *tlb_hit_count) {       
  int tlb_index = tlbContains(page);
  if (tlb_index != -1) {
    (*tlb_hit_count)++;
    return tlb[tlb_index][1];
  }

 
  if (table[page]!=-1 && queue[table[page]]==page) { 
    tlbUpdate(page);
    return table[page];
  }

  int offset = (logic_add / FRAME_SIZE) * FRAME_SIZE;
  fseek(osos, offset, 0);      

  int available_frame = get_available_frame(page);
  fread(&main_mem_fifo[available_frame*FRAME_SIZE],sizeof(char),256,osos);
  table[page] = available_frame;
  (*page_fault_count)++;
  tlbUpdate(page);
  return table[page];
}

void question1();
void question2();

int main(int argc, const char* argv[]) {
  for (int i=0; i<5; ++i){
    pageCount[i]=0; pageCount2[i]=0;
    tlbCount[i]=0; tlbCount2[i]=0;
    count[i]=0; count2[i]=0;
  }

  question1(); 
  question2();

  printf("\nPart 1 Statistics (256 frames):\n");
  printf("access count\ttlb hit count\tpage fault count\ttlb hit rate\tpage fault rate\n");
  for (int i=0; i<5; ++i){
    printf("%d\t\t%d\t\t%d\t\t\t%.4f\t\t%.4f\n", count[i], tlbCount[i], pageCount[i],
           1.0f*tlbCount[i]/count[i], 1.0f*pageCount[i]/count[i]);
  }

  printf("\nPart 2 Statistics (128 frames):\n");
  printf("access count\ttlb hit count\tpage fault count\ttlb hit rate\tpage fault rate\n");
  for (int i=0; i<5; ++i){
    printf("%d\t\t%d\t\t%d\t\t\t%.4f\t\t%.4f\n", count2[i], tlbCount2[i], pageCount2[i],
           1.0f*tlbCount2[i]/count2[i], 1.0f*pageCount2[i]/count2[i]);
  }
  printf("\n\t\tDONE!\n");
  return 0;
}

void question1(){
  FILE* fadd = fopen("addresses.txt", "r");  
  if (fadd == NULL) { fprintf(stderr, "Could not open file: 'addresses.txt'\n");  exit(FILE_ERROR);  }

  FILE* fcorr = fopen("correct.txt", "r"); 
  if (fcorr == NULL) { fprintf(stderr, "Could not open file: 'correct.txt'\n");  exit(FILE_ERROR);  }

  char buf[BUFLEN];
  unsigned   page, offset, physical_add, frame = 0;
  unsigned   logic_add;                  
  unsigned   virt_add, phys_add, value;  

  osos = fopen("BACKING_STORE.bin", "rb");   
  if (osos == NULL) { fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");  exit(FILE_ERROR);  }
  
  for (int i=0; i<256; ++i) { table[i]=-1; }
  for (int i=0; i<16; ++i) { tlb[i][0]=-1; }
  
  int access_count=0, page_fault_count=0, tlb_hit_count=0;
  FrameNow = 0;
  tlbEntry = 0;

  while (fscanf(fadd, "%d", &logic_add) != EOF) {
    ++access_count;

    fscanf(fcorr, "%s %s %d %s %s %d %s %d", buf, buf, &virt_add,
           buf, buf, &phys_add, buf, &value); 

    page   = getpage(  logic_add);
    offset = getoffset(logic_add);
    frame = getframe(logic_add, page, &page_fault_count, &tlb_hit_count);

    physical_add = frame * FRAME_SIZE + offset;
    int val = (int)(main_mem[physical_add]);
    fseek(osos, logic_add, 0);

    if (access_count > 0 && access_count%200==0){
      tlbCount[(access_count/200)-1] = tlb_hit_count;
      pageCount[(access_count/200)-1] = page_fault_count;
      count[(access_count/200)-1] = access_count;
    }
    
    printf("logical: %5u (page: %3u, offset: %3u) ---> physical: %5u -> value: %d-- passed\n", 
            logic_add, page, offset, physical_add, val);
    if (access_count % 5 == 0) { printf("\n"); }

    assert(physical_add == phys_add);
    assert(value == val);
  }
  tlbCount[4] = tlb_hit_count;
  pageCount[4] = page_fault_count;
  count[4] = access_count;
  fclose(fcorr);
  fclose(fadd);
  fclose(osos);
  
  printf("ALL logical ---> physical assertions PASSED!\n");
  printf("ALL read memory value assertions PASSED!\n"); 

  printf("\n\t\t...Part 1 done.\n");
}

void question2(){
  char buf[BUFLEN];
  unsigned   page, offset, physical_add, frame = 0;
  unsigned   logic_add;               
  unsigned   virt_add, phys_add, value; 

  printf("\n Start Part 2...\n");

  for (int i=0; i<256; ++i) { table[i]=-1; }
  for (int i=0; i<16; ++i) { tlb[i][0]=-1; }
  for (int i=0; i<128; ++i) { queue[i]=-1; }
  
  int access_count=0, page_fault_count=0, tlb_hit_count=0;
  head=0; tail=0;

  FILE* fadd = fopen("addresses.txt", "r"); 
  if (fadd == NULL) { fprintf(stderr, "Could not open file: 'addresses.txt'\n");  exit(FILE_ERROR);  }

  FILE* fcorr = fopen("correct.txt", "r");     
  if (fcorr == NULL) { fprintf(stderr, "Could not open file: 'correct.txt'\n");  exit(FILE_ERROR);  }

  osos = fopen("BACKING_STORE.bin", "rb");   
  if (osos == NULL) { fprintf(stderr, "Could not open file: 'BACKING_STORE.bin'\n");  exit(FILE_ERROR);  }

  while (fscanf(fadd, "%d", &logic_add) != EOF) {
    ++access_count;

    fscanf(fcorr, "%s %s %d %s %s %d %s %d", buf, buf, &virt_add,
           buf, buf, &phys_add, buf, &value);  

 
    page   = getpage(  logic_add);
    offset = getoffset(logic_add);
    frame = getframe_fifo(logic_add, page, &page_fault_count, &tlb_hit_count);

    physical_add = frame * FRAME_SIZE + offset;
    int val = (int)(main_mem_fifo[physical_add]);
    fseek(osos, logic_add, 0);

    if (access_count > 0 && access_count%200==0){
      tlbCount2[(access_count/200)-1] = tlb_hit_count;
      pageCount2[(access_count/200)-1] = page_fault_count;
      count2[(access_count/200)-1] = access_count;
    }
    
    printf("logical: %5u (page: %3u, offset: %3u) ---> physical: %5u -> value: %d-- passed\n", 
            logic_add, page, offset, physical_add, val);
    if (access_count % 5 == 0) { printf("\n"); }

    assert(value == val);
  }
  tlbCount2[4] = tlb_hit_count;
  pageCount2[4] = page_fault_count;
  count2[4] = access_count;
  fclose(fcorr);
  fclose(fadd);
  fclose(osos);

  printf("ALL read memory value assertions PASSED!\n");
  printf("\n\t\t...Part 2 done.\n");
}