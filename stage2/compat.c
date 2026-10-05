/* compat -- brute-force test that an abstraction phi is compatible with the
 * moves: phi(s.m) must depend only on (phi(s), m). Also counts distinct
 * abstract states. usage: ./compat FAMILY [K]   (families: see candidates.md)
 */
#include "oracle_lib.h"
static int K; static char FAM;
static uint32_t phi(const state_t *s){
  uint8_t pos[7]; for(int i=0;i<7;i++) pos[s->p[i]]=i;
  uint32_t key=0, pr=0, orank=0;
  for(int i=0;i<6;i++) orank=orank*3+s->o[i];
  pr = rank_state(s)/729;
  switch(FAM){
  case 'A': return orank;
  case 'B': return pr;
  case 'C': for(int c=0;c<K;c++) key=key*7+pos[c]; return key;
  case 'D': for(int c=0;c<K;c++) key=(key*7+pos[c])*3+s->o[pos[c]]; return key;
  case 'E': for(int c=0;c<K;c++) key=key*7+pos[c]; return key*729+orank;
  case 'F': for(int c=0;c<K;c++) key=key*3+s->o[pos[c]]; return key*5040+pr;
  case 'G': for(int i=0;i<K;i++) key=key*7+s->p[i]; return key;   /* contents of positions */
  case 'H': for(int i=0;i<K;i++) key=key*3+s->o[i]; return key;   /* orientation at positions */
  case 'P': { int inv=0; for(int i=0;i<7;i++) for(int j=i+1;j<7;j++) inv+=s->p[i]>s->p[j]; return inv&1; }
  }
  return 0;
}
int main(int argc,char**argv){
  FAM=argv[1][0]; K=argc>2?atoi(argv[2]):0;
  uint64_t space=1; switch(FAM){case 'A':space=729;break;case 'B':space=5040;break;case 'P':space=2;break;
   case 'C':case 'G': for(int i=0;i<K;i++) space*=7; break;
   case 'D': for(int i=0;i<K;i++) space*=21; break;
   case 'E': space=729; for(int i=0;i<K;i++) space*=7; break;
   case 'F': space=5040; for(int i=0;i<K;i++) space*=3; break;
   case 'H': for(int i=0;i<K;i++) space*=3; break;}
  int32_t *img=malloc(space*MOVES*4); uint8_t *seen=calloc(space,1);
  memset(img,0xff,space*MOVES*4);
  uint64_t bad=0, distinct=0; state_t s;
  for(uint32_t r=0;r<STATES;r++){ unrank_state(r,&s); uint32_t a=phi(&s);
    if(!seen[a]){seen[a]=1;distinct++;}
    for(int m=0;m<MOVES;m++){ state_t t=apply_move(s,m); int32_t b=phi(&t);
      int32_t *slot=&img[(uint64_t)a*MOVES+m]; if(*slot<0)*slot=b; else if(*slot!=b) bad++; } }
  printf("%c%d distinct=%llu fiber=%.1f compatible=%s\n",FAM,K,(unsigned long long)distinct,(double)STATES/distinct,bad?"NO":"yes");
}
