# !/bin/bash

cd ./load_awareness_module
echo 'shanqian' | sudo -S make
sudo insmod load_awareness_module.ko