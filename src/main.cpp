
//==============================================================
// Copyright © 2019 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#include <chrono>
#include <iomanip>
#include <iostream>
#include <CL/sycl.hpp>

#include "dpc_common.hpp"

using namespace std;
using namespace cl::sycl;

void ShowDevice(queue &q) {
  // Output platform and device information.
  auto device = q.get_device();
  auto p_name = device.get_platform().get_info<info::platform::name>();
  cout << std::setw(20) << "Platform Name: " << p_name << "\n";
  auto p_version = device.get_platform().get_info<info::platform::version>();
  cout << std::setw(20) << "Platform Version: " << p_version << "\n"; 
  auto d_name = device.get_info<info::device::name>();
  cout << std::setw(20) << "Device Name: " << d_name << "\n";
  auto max_work_group = device.get_info<info::device::max_work_group_size>();
  cout << std::setw(20) << "Max Work Group: " << max_work_group << "\n";
  auto max_compute_units = device.get_info<info::device::max_compute_units>();
  cout << std::setw(20) << "Max Compute Units: " << max_compute_units << "\n";
}

void Execute(queue &q) {
  // run our work loads here (calculate running average of the tick prices for the stock
}

void Usage(string program_name) {
  // Utility function to display argument usage
  cout << " Incorrect parameters\n";
  cout << " Usage: ";
  cout << program_name << "\n\n";
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 1) {
    Usage(argv[0]);
  }

  try {

    // Create a queue using default device
    // Set the SYCL_DEVICE_FILTER, we are using PI_OPENCL environment variable
      
    // Default queue, set accelerator choice above.
    //queue q (default_selector{},dpc_common::exception_handler);
    queue q (gpu_selector{},dpc_common::exception_handler);
    //queue q (accelerator_selector{},dpc_common::exception_handler);
    // Display the device info
    ShowDevice(q);
    // launch the body of the application
    Execute(q);
  } catch (...) {
    // some other exception detected
    cout << "Failure\n";
    terminate();
  }
  cout << "Success\n";
  return 0;
}
