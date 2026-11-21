#include <math.h>
#include <stdio.h>
#define D(f) double k_##f(double);
D(cbrt) D(acosh) D(asinh) D(atanh) D(erf) D(erfc) D(expm1) D(log1p) D(lgamma) D(tgamma) D(rint) D(logb)
long k_lround(double); double k_remainder(double,double); double k_copysign(double,double); double k_fmax(double,double); int k_ilogb(double);
int main(){ double xs[]={-7.3,-2.5,-1.0,-0.4,-1e-9,0.0,1e-9,0.3,0.5,0.9,1.0,1.5,2.5,3.7,10.0,100.0,170.2};
 double worst=0; const char* wn="";
 #define C(n,ref,mine) { double r=ref, m=mine; if(!isnan(r)||!isnan(m)){ double e=fabs(r-m)/(fabs(r)>1?fabs(r):1); if(isinf(r)&&r==m) e=0; if(e>worst){worst=e;wn=n;} if(e>1e-9) printf("%s(%g): ref %.17g mine %.17g\n",n,x,r,m);} }
 for(int i=0;i<17;i++){ double x=xs[i];
  C("cbrt",cbrt(x),k_cbrt(x)); C("asinh",asinh(x),k_asinh(x)); if(x>=1) C("acosh",acosh(x),k_acosh(x)); if(fabs(x)<1) C("atanh",atanh(x),k_atanh(x));
  C("erf",erf(x),k_erf(x)); C("erfc",erfc(x),k_erfc(x)); C("expm1",expm1(x),k_expm1(x)); if(x>-1) C("log1p",log1p(x),k_log1p(x));
  if(!(x<=0&&x==floor(x))){ C("lgamma",lgamma(x),k_lgamma(x)); C("tgamma",tgamma(x),k_tgamma(x)); }
  C("rint",rint(x),k_rint(x)); C("lround",(double)lround(x),(double)k_lround(x)); C("remainder",remainder(x,0.7),k_remainder(x,0.7)); if(x!=0){C("logb",logb(x),k_logb(x)); C("ilogb",(double)ilogb(x),(double)k_ilogb(x));}
 }
 printf("lround(100)=%ld fmax(nan,1)=%g copysign(3,-0.)=%g worst=%g (%s)\n",k_lround(100.0),k_fmax(NAN,1),k_copysign(3,-0.0),worst,wn);}
