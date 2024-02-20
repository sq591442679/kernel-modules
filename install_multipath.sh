# !/bin/bash

cd ./multipath_module
echo 'shanqian' | sudo -S make
sudo insmod multipath_module.ko