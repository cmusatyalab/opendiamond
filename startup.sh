cd $USER/opendiamond
python setup.py install 
systemctl restart diamondd
systemctl restart dataretriever
#systemctl is-active --quiet diamondd #&& echo "diamondd is running" || echo "diamondd is NOT running"
systemctl is-active diamondd #&& echo "diamondd is running" || echo "diamondd is NOT running"
systemctl is-active dataretriever #&& echo "dataretriever is running" || echo "dataretriever is NOT running"

