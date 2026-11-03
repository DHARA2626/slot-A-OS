#include <stdio.h>
#define N 4
const char *nm[N]={"P1","P2","P3","P4"};
int at[N]={0,1,2,3}, bt[N]={8,4,9,5}, pr[N]={3,1,4,2}; /* lower number = higher priority */
int ct[N], rt[N];
static void report(const char *t){
  double a=0,w=0,r=0;
  printf("\n%s\nProc  CT  TAT  WT  RT\n",t);
  for(int i=0;i<N;i++){int tat=ct[i]-at[i],wt=tat-bt[i];a+=tat;w+=wt;r+=rt[i];
    printf("%-4s %3d %4d %3d %3d\n",nm[i],ct[i],tat,wt,rt[i]);}
  printf("Avg TAT=%.2f  Avg WT=%.2f  Avg RT=%.2f\n",a/N,w/N,r/N);
}
/* non-preemptive: mode 0=FCFS, 1=SJF, 2=Priority */
static void nonpre(int mode,const char*t){
  int d[N]={0},t0=0,cnt=0;printf("\n%s Gantt:",t);
  while(cnt<N){int b=-1;
    for(int i=0;i<N;i++){ if(d[i]||at[i]>t0)continue;
      if(b<0){b=i;continue;}
      int key_i= mode==0?at[i]:mode==1?bt[i]:pr[i];
      int key_b= mode==0?at[b]:mode==1?bt[b]:pr[b];
      if(key_i<key_b)b=i;}
    if(b<0){t0++;continue;}
    rt[b]=t0-at[b];printf(" %s(%d-%d)",nm[b],t0,t0+bt[b]);
    t0+=bt[b];ct[b]=t0;d[b]=1;cnt++;}
  printf("\n");report(t);
}
static void srtf(void){
  int left[N],first[N],t0=0,cnt=0,last=-1,ss=0;
  for(int i=0;i<N;i++){left[i]=bt[i];first[i]=-1;}
  printf("\nSRTF Gantt:");
  while(cnt<N){int b=-1;
    for(int i=0;i<N;i++) if(left[i]>0&&at[i]<=t0&&(b<0||left[i]<left[b]))b=i;
    if(b<0){t0++;continue;}
    if(b!=last){ if(last>=0&&ss<t0)printf(" %s(%d-%d)",nm[last],ss,t0); ss=t0; last=b;}
    if(first[b]<0)first[b]=t0-at[b];
    left[b]--;t0++;
    if(left[b]==0){cnt++;ct[b]=t0;}
  }
  printf(" %s(%d-%d)\n",nm[last],ss,t0);
  for(int i=0;i<N;i++)rt[i]=first[i];
  report("SRTF");
}
int main(void){
  nonpre(0,"FCFS");nonpre(1,"SJF (non-preemptive)");srtf();nonpre(2,"Priority (non-preemptive, P1=3 P2=1 P3=4 P4=2)");
  return 0;}
