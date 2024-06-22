#!/bin/bash

if [ ! -d "juliet" ]; then
	wget https://samate.nist.gov/SARD/downloads/test-suites/2017-10-01-juliet-test-suite-for-c-cplusplus-v1-3.zip
	unzip 2017-10-01-juliet-test-suite-for-c-cplusplus-v1-3.zip
	mv C juliet
fi

./juliet.py -g 415
./juliet.py -g 416
./juliet.py -m 415
./juliet.py -m 416

