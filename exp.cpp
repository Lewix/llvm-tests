long exp(long e, long n)
{
  long result = 1;
	for (long i = 0; i < n; i++) {
		result *= e;
  }
  return result;
}
