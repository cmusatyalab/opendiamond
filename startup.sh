services=("diamondd" "dataretriever" "docker")
python setup.py install 
systemctl restart diamondd
systemctl restart dataretriever
for s in ${services[*]}
  do
    systemctl is-active $s >/dev/null 2>&1 && echo $s" is active" || echo $s" NOT active"
  done  
