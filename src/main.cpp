
//==============================================================
// Copyright Â© 2019 Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

// oneDPL headers should be included before standard headers
#include <oneapi/dpl/algorithm>
#include <oneapi/dpl/execution>
#include <oneapi/dpl/iterator>

#include <chrono>
#include <iomanip>
#include <iostream>
#include <CL/sycl.hpp>
#include <fstream>
#include <experimental/filesystem>
#include <numeric>

// boost
#include <boost/assert.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/program_options.hpp>
#include <boost/scope_exit.hpp>

#include <execution>
#include "dpc_common.hpp"
#include "node.hpp"

using namespace std;
using namespace cl::sycl;
using namespace node;

namespace fs = std::experimental::filesystem;
namespace po = boost::program_options;

SymbolFiles symFileList;
Symbols symbols;
std::unordered_map<std::string, FileId> fileIdMap;

std::ostream& operator<<(std::ostream& ostr, Node& nd) {
    auto tm = nd.m_tm / 1000;
    auto millis = nd.m_tm % 1000;
    std::tm ptm = *std::localtime(&tm);
    char dt[64], buffer[32];
    std::strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", &ptm);
    snprintf(dt, 64, "%s.%03ld", buffer, millis);
    ostr << symbols[nd.m_fId] << "," << dt << "," << nd.m_px << "," << nd.m_sz << "," << nd.m_exch << "," << getTypeStr(nd.m_type);
    return ostr;
}

class FileHandler {
private:
    std::ifstream m_ifs;
    FileId m_fId = 0;

    uint_fast64_t getMillis(const std::string& tm) {
	auto indx = tm.find_last_of(".");
	if (indx == std::string::npos) return 0;
	return std::stoll(tm.substr(indx + 1));
    }

public:
    ~FileHandler() { m_ifs.close(); }
    FileHandler(const std::string& fname, const FileId fId):m_fId(fId) {
	// std::cout << "Opening " << fname << std::endl;
	m_ifs.open(fname);
    }

    NodePtr getNextNode() {
	if (not m_ifs.good()) return nullptr;
	std::string line;
	while(std::getline(m_ifs, line)) {
	    boost::algorithm::trim(line);
	    if (std::empty(line)) continue;
	    if (boost::algorithm::starts_with(line, "#")) continue;
	    if (boost::algorithm::starts_with(line, "//")) continue;

	    std::vector<std::string> args;
	    boost::split(args, line, boost::is_any_of(","));
	    BOOST_ASSERT(args.size() == 5);

	    auto node = std::make_shared<Node>();
	    std::tm mkTm = {};
	    strptime(args[0].c_str(), "%Y-%m-%d %H:%M:%S", &mkTm);

	    node->m_tm = (std::mktime(&mkTm) * 1000) + getMillis(args[0]);
	    node->m_px = std::stod(args[1]);
	    node->m_sz = std::stoll(args[2]);
	    node->m_exch = args[3];
	    node->m_type = getType(args[4]);
	    node->m_fId = m_fId;
	    return node;
	}
	return nullptr;
    }

    bool getNextNode(NodePtr& node) {
	if (not m_ifs.good()) return false;
	std::string line;
	while(std::getline(m_ifs, line)) {
	    boost::algorithm::trim(line);
	    if (std::empty(line)) continue;
	    if (boost::algorithm::starts_with(line, "#")) continue;
	    if (boost::algorithm::starts_with(line, "//")) continue;

	    std::vector<std::string> args;
	    boost::split(args, line, boost::is_any_of(","));
	    BOOST_ASSERT(args.size() == 5);

	    std::tm mkTm = {};
	    strptime(args[0].c_str(), "%Y-%m-%d %H:%M:%S", &mkTm);

	    node->m_tm = (std::mktime(&mkTm) * 1000) + getMillis(args[0]);
	    node->m_px = std::stod(args[1]);
	    node->m_sz = std::stoll(args[2]);
	    node->m_exch = args[3];
	    node->m_type = getType(args[4]);
	    node->m_fId = m_fId;
	    return true;
	}
	return false;
    }

};

std::vector<std::unique_ptr<FileHandler>> fileHandles;

std::string getSymbol(const std::string& fname) {
    auto index = fname.find_last_of(".");
    if (std::string::npos == index) return fname;
    return fname.substr(0, index);
}

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

int getMaxComputeUnits(queue& q) {
  auto device = q.get_device();
  return device.get_info<info::device::max_compute_units>();
}

FileId initialize(const std::string& path) {
    // NULL FileId placeholder
    symFileList.emplace_back(""); 
    symbols.emplace_back("");
    fileHandles.emplace_back(nullptr);
    FileId fId = NULL_FILEID;
    for (const auto & entry : fs::directory_iterator(path)) {
	if (fs::is_regular_file(entry) && entry.path().extension() == ".txt") {
	    const auto sym = getSymbol(entry.path().filename());
	    symFileList.emplace_back(entry.path());
	    symbols.emplace_back(sym);
	    fileIdMap.emplace(entry.path(), ++fId);
	    fileHandles.emplace_back(std::make_unique<FileHandler>(entry.path(), fId));
	}
    }
    const FileId MAX_FILEID = ++fId;
    BOOST_ASSERT(symbols.size() == MAX_FILEID);

    int j = 0;
    for (const auto& i : fileIdMap) {
	BOOST_ASSERT(i.first == symFileList[i.second]);
    }

    std::cout << "MAX FILE ID " << MAX_FILEID << std::endl;
    return MAX_FILEID;
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

const auto mktdir = "/home/u172990/prjs/mktgen/mkt";
const auto maxticks = 2000;

double calculateWtdAvg(const std::vector<double>& wtdPx, const uint_fast64_t wts) {
  // double sum = std::accumulate(wtdPx.begin(), wtdPx.end(), 0);
  auto policy = oneapi::dpl::execution::dpcpp_default;
  double sum = std::reduce(wtdPx.begin(), wtdPx.end());
  return sum / wts;
}

void Process(queue& q, const std::string& path, std::vector<double>& avgWtdPx) {
    const FileId MAX_FILEID = initialize(path);
    avgWtdPx.reserve(MAX_FILEID);

    std::vector<double> wtdPx;
    wtdPx.reserve(maxticks); // @TODO num of lines?
    uint_fast64_t wts = 0;
    NodePtr nd = std::make_shared<Node>();
    
    for (auto i = 1; i < MAX_FILEID; ++i) {
      int cnt = 0;
      while (fileHandles[i]->getNextNode(nd) && (++cnt < maxticks)) {
	wtdPx.emplace_back(nd->m_sz * nd->m_px);
	wts += nd->m_sz;
      }
      //      std::cout << "Symbol [" << Symbol[i] << "] weighted avg [" << calculateWtdAvg(wtdPx, wts) << "]" << std::endl;
      avgWtdPx.emplace_back(calculateWtdAvg(wtdPx, wts));
      wtdPx.clear();
      wts = 0;
    }
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
    queue q (default_selector{},dpc_common::exception_handler);
    //queue q (accelerator_selector{},dpc_common::exception_handler);
    // Display the device info
    ShowDevice(q);
    // launch the body of the application
    //Execute(q);

    std::vector<double> avgWtdPx;
    dpc_common::MyTimer t_ser;
    Process(q, mktdir, avgWtdPx);
    dpc_common::Duration serial_time = t_ser.elapsed();

    std::cout << avgWtdPx.size() << std::endl;
    // Report the results
    std::cout << std::setw(20) << "serial time: " << serial_time.count() << "s\n";
    for (int i = 0; i < avgWtdPx.size(); ++i) {
      std::cout << "Symbol [" << symbols[i + 1] << "] weighted avg [" << avgWtdPx[i] << "]" << std::endl;
    }
  } catch (...) {
    // some other exception detected
    cout << "Failure\n";
    terminate();
  }
  cout << "Success\n";
  return 0;
}
