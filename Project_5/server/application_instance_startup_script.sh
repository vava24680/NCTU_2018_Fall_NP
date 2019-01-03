#!/bin/bash
cd /home/ubuntu
curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
python3 get-pip.py
curl -O http://npaws.haohao.in:8080/application_server.tar
tar -xvf application_server.tar
cd application_server
pip3 install -I -r application_server_requirement.txt
chown -R ubuntu:ubuntu ./
python3 -u application_server.py 0.0.0.0 10778 > application_server.log
