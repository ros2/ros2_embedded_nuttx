NM=arm-none-eabi-nm
echo "TOP 20 BIG DATA"
$NM --print-size --size-sort --radix dec -C nuttx | grep ' [DdBb] ' | tail -20
echo "TOP 20 BIG CODE"
$NM --print-size --size-sort --radix dec -C nuttx | grep ' [TtWw] ' | tail -20
