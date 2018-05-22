#!usr/bin
start_T=`date +%s`

speed='fast'
for qos in 0 1 2; do
    name=${speed}_${qos}_${start_T}
    python ./Inspector.py --speed ${speed} --qos ${qos} > ./Log/${name} 2>&1 &
done

speed='SYS'
qos=2
name=${speed}_${qos}_${start_T}
python ./Inspector.py --speed ${speed} --qos ${qos} > ./Log/${name} 2>&1 &
