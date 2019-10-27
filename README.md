Interactive search of non-indexed data <http://diamond.cs.cmu.edu/>

Installing the opendiamond backend (not including C libfilter and scope cookie hook) is now purely Pythonic.

```bash
conda env create -n <VIRTUAL_ENV_NAME> -f environment.yml
conda activate <VIRTUAL_ENV_NAME>
python setup.py install
# start the backend
diamondd
```

The virtual env's Python interpreter path is "baked in" in the entry points. So one can also run directly without `conda activate`:
```bash
/path_to_virtual_env/bin/diamondd
```
