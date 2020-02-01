cd $USER/opendiamond
python setup.py install 
systemctl restart diamondd
systemctl restart dataretriever
systemctl is-active --quiet diamondd && echo diamondd is running
systemctl is-active --quiet dataretriever && echo dataretriever is running

