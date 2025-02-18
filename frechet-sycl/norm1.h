double norm1(int i, int j, const double *c1, const double *c2)
{
  double dist, diff; /* Temp variables for simpler computations */
  int k; /* Index for iterating over dimensions */

  /* Initialise distance */
  dist = 0.0;

  for (k = 0; k < n_d; k++)
  {
    /*
     * Compute the distance between the k-th component of the i-th point
     * of the 1st curve and the k-th component of the j-th point of the
     * 2nd curve.
     *
     * Notice the 1-offset added for better readability (as in [1]).
     */
    diff = *(c1 + (i - 1)*n_d + k) - *(c2 + (j - 1)*n_d + k);
    /* Increment the accumulator variable with the absolute distance */
    dist += fabs(diff);
  }

  return dist;
}

double recursive_norm1(int i, int j, int n_2, double *ca,
                       const double *c1, const double *c2)
{
  /*
   * Target the shortcut to the (i, j)-th entry of the matrix `ca`
   *
   * Once again, notice the 1-offset.
   */
  double *ca_ij = ca + (i - 1)*n_2 + (j - 1);

  /* This implements the algorithm from [1] */
  if (*ca_ij > -1.0) 
  {
    return *ca_ij;
  }
  else if ((i == 1) && (j == 1))
  {
    *ca_ij = norm1(1, 1, c1, c2);
  }
  else if ((i > 1) && (j == 1))
  {
    *ca_ij = sycl::fmax(recursive_norm1(i - 1, 1, n_2, ca, c1, c2), norm1(i, 1, c1, c2));
  }
  else if ((i == 1) && (j > 1))
  {
    *ca_ij = sycl::fmax(recursive_norm1(1, j - 1, n_2, ca, c1, c2), norm1(1, j, c1, c2));
  }
  else if ((i > 1) && (j > 1))
  {
    *ca_ij = sycl::fmax(
        sycl::fmin(sycl::fmin(
            recursive_norm1(i - 1, j    , n_2, ca, c1, c2),
            recursive_norm1(i - 1, j - 1, n_2, ca, c1, c2)),
            recursive_norm1(i,     j - 1, n_2, ca, c1, c2)),
        norm1(i, j, c1, c2));
  }
  else
  {
    *ca_ij = INFINITY;
  }

  return *ca_ij;
}

void distance_norm1 (
  nd_item<2> &item,
  int n_1, int n_2,
  double *__restrict ca,
  const double *__restrict c1,
  const double *__restrict c2)
{
  int i = item.get_global_id(1);
  int j = item.get_global_id(0);
  if (j >= 1 && j <= n_2 && i >= 1 && i <= n_1)
    recursive_norm1(i, j, n_2, ca, c1, c2);
}

