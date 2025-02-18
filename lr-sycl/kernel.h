void linear_regression(
  nd_item<1> &item,
  const float2 *__restrict dataset,
  float4 *__restrict interns,
  float4 *__restrict result)
{
  size_t glob_id  = item.get_global_id(0);
  size_t loc_id   = item.get_local_id(0);
  size_t loc_size = item.get_local_range(0); 

  /* Initialize local buffer */
  interns[loc_id].x() = dataset[glob_id].x();
  interns[loc_id].y() = dataset[glob_id].y();
  interns[loc_id].z() = (dataset[glob_id].x() * dataset[glob_id].y());
  interns[loc_id].w() = (dataset[glob_id].x() * dataset[glob_id].x());
  
  item.barrier(access::fence_space::local_space);

  for (size_t i = (loc_size / 2), old_i = loc_size; i > 0; old_i = i, i /= 2)
  {
    if (loc_id < i) {
      // Only first half of workitems on each workgroup
      interns[loc_id] += interns[loc_id + i];
      if (loc_id == (i - 1) && old_i % 2 != 0) {
        // If there is an odd number of data
        interns[loc_id] += interns[old_i - 1];
      }
    }
    item.barrier(access::fence_space::local_space);
  }

  if (loc_id == 0) result[item.get_group(0)] = interns[0];
}

void rsquared(
  nd_item<1> &item,  
  const float2 *__restrict dataset,
  const float mean,
  const float2 equation, // [a0,a1]
  float2 *__restrict dist,
  float2 *__restrict result)
{
  size_t glob_id  = item.get_global_id(0);
  size_t loc_id   = item.get_local_id(0);
  size_t loc_size = item.get_local_range(0); 

  dist[loc_id].x() = sycl::pow((dataset[glob_id].y() - mean), 2.f);

  float y_estimated = dataset[glob_id].x() * equation.y() + equation.x();
  dist[loc_id].y() = sycl::pow((y_estimated - mean), 2.f);

  item.barrier(access::fence_space::local_space);

  for (size_t i = (loc_size / 2), old_i = loc_size; i > 0; old_i = i, i /= 2)
  {
    if (loc_id < i) {
      // Only first half of workitems on each workgroup
      dist[loc_id] += dist[loc_id + i];
      if (loc_id == (i - 1) && old_i % 2 != 0) {
        // If there is an odd number of data
        dist[loc_id] += dist[old_i - 1];
      }
    }
    item.barrier(access::fence_space::local_space);
  }

  if (loc_id == 0) result[item.get_group(0)] = dist[0];
}
