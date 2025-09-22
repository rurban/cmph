#include "miller_rabin.h"

static inline cmph_uint64 int_pow(cmph_uint64 a, cmph_uint64 d, cmph_uint64 n)
{
	cmph_uint64 a_pow = a;
	cmph_uint64 res = 1;
	while(d > 0)
	{
		if((d & 1) == 1)
			res =(((cmph_uint64)res) * a_pow) % n;
		a_pow = (((cmph_uint64)a_pow) * a_pow) % n;
		d /= 2;
	};
	return res;
};

static inline cmph_uint8 check_witness(cmph_uint64 a_exp_d, cmph_uint64 n, cmph_uint64 s)
{
	cmph_uint64 i;
	cmph_uint64 a_exp = a_exp_d;
	if(a_exp == 1 || a_exp == (n - 1))
		return 1;
	for(i = 1; i < s; i++)
	{
		a_exp = (((cmph_uint64)a_exp) * a_exp) % n;
		if(a_exp == (n - 1))
			return 1;
	};
	return 0;
};

cmph_uint8 check_primality(cmph_uint64 n)
{
	cmph_uint64 a, d, s, a_exp_d;
	cmph_uint64 low_primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29,
				    31, 37, 41, 43, 47};
	for (unsigned i=0; i < sizeof(low_primes)/sizeof(*low_primes); i++)
	{
		if ((n % low_primes[i]) == 0)
			return 0;
	}
	//we decompose the number n - 1 into 2^s*d
	s = 0;
	d = n - 1;
	do
	{
		s++;
		d /= 2;
	}while((d % 2) == 0);

	a = 2;
	a_exp_d = int_pow(a, d, n);
	if(check_witness(a_exp_d, n, s) == 0)
		return 0;
	a = 7;
	a_exp_d = int_pow(a, d, n);
	if(check_witness(a_exp_d, n, s) == 0)
		return 0;
	a = 61;
	a_exp_d = int_pow(a, d, n);
	if(check_witness(a_exp_d, n, s) == 0)
		return 0;
	return 1;
};


