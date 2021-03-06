@echo off
rem this assumes that 'release adolc', 'release betadiff', and 'release cppad' have already been built successfully
rem this script runs in directory TestCases\primary\HOK

rem cd CASAL
rem del params_est.out
rem ..\..\..\CASAL\casal.exe -e -o params_est.out > run_estimation.txt 2>&1

rem cd ..\CASAL_sens1
rem del params_est.out
rem ..\..\..\CASAL\casal.exe -e -o params_est.out > run_estimation.txt 2>&1


cd Casal2\betadiff_casal_flags_off
del params_est.out
..\..\..\..\..\BuildSystem\bin\windows\release_betadiff\casal2 -e -o params_est.out > run_estimation.txt 2>&1

cd ..\betadiff_casal_flags_on
del params_est.out
..\..\..\..\..\BuildSystem\bin\windows\release_betadiff\casal2 -e -o params_est.out > run_estimation.txt 2>&1

cd ..\betadiff_casal_flags_on_low_tol
del params_est.out
..\..\..\..\..\BuildSystem\bin\windows\release_betadiff\casal2 -e -o params_est.out > run_estimation.txt 2>&1


cd ..\cppad_casal_flags_off
del params_est.out
..\..\..\..\..\BuildSystem\bin\windows\release_cppad\casal2 -e -o params_est.out > run_estimation.txt 2>&1

cd ..\cppad_casal_flags_on
del params_est.out
..\..\..\..\..\BuildSystem\bin\windows\release_cppad\casal2 -e -o params_est.out > run_estimation.txt 2>&1


cd ..\adolc_casal_flags_off
del params_est.out
..\..\..\..\..\BuildSystem\bin\windows\release_adolc\casal2 -e -o params_est.out > run_estimation.txt 2>&1

cd ..\adolc_casal_flags_on
del params_est.out
..\..\..\..\..\BuildSystem\bin\windows\release_adolc\casal2 -e -o params_est.out > run_estimation.txt 2>&1

cd ..\adolc_casal_flags_on_low_tol
del params_est.out
..\..\..\..\..\BuildSystem\bin\windows\release_adolc\casal2 -e -o params_est.out > run_estimation.txt 2>&1

