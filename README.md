# IntelOneApiPOC
Using dpcpp (data parallel C++, Intel's C++ compiler for SYCL) to do some basic GPU workload

Run the following command for getting the GPUs

qsub -I -l nodes=1:gpu:ppn=2 -d .

qsub -I -l nodes=1:gen9:ppn=2 -d .

qsub -I -l nodes=1:iris_xe_max:quad_gpu:ppn=2 -d .

qsub -I -l nodes=1:iris_xe_max:dual_gpu:ppn=2 -d .

qsub -I -l nodes=1:iris_xe_max:ppn=2 -d .

'-I' indicates an interactive session where we can run our application and it will run on the GPU requested. Instead to sub a job we can just invoke 'qsub -l nodes=1:gpu:ppn=2 <script>'. It will output a job id and we can delete that job if needed using qdel.


More information on https://devcloud.intel.com/oneapi/documentation/job-submission/