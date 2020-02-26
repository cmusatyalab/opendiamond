#!/usr/bin/python
import os 
import random
import math
import pwd
import grp
import sys
import logging
import glob
from os.path import join as osp

_log = logging.getLogger(__name__)

def read_file_list(path):
    _log.info("Reading {}".format(path))
    sys.stdout.flush()
    if not os.path.exists(path):
        sys.exit('Error: Path {} does not exist'.format(path))

    with open(path,'r') as f:
        data = f.read().splitlines()
    return [d.strip() for d in data]


def sample_data(data, num, seed):
    num = int(math.ceil(num))
    random.Random(seed).shuffle(data)
    return data[:num], data[num:]

def write_data(path, lists_, seed, convert=False, base_dir=None):
    _log.info("Writing data for path {}".format(path))
    uid = pwd.getpwnam("dataretriever").pw_uid
    gid = grp.getgrnam("dataretriever").gr_gid
    data = []
    for l in lists_:
        data.extend(l)
    random.Random(seed).shuffle(data)
    if convert:
        path = path.upper()
    if base_dir:
        path = osp(base_dir, path)
    with open(path,'w') as f:
        for d in data:
            print(d, file=f)
    os.chown(path, uid, gid)

def split_data(base_dir, base_rate=0.05, seed=42):
    _log.info("Dir {} Base: {} Seed:{}".format(base_dir, base_rate, seed))
    sys.stdout.flush()
    
    if not os.path.exists(osp(base_dir, 'stream_inat')):
        _log.info("Creating data files...")
        sys.stdout.flush()
        get_data_files(base_dir)

    inat = read_file_list(osp(base_dir, 'stream_inat'))
    google = read_file_list(osp(base_dir, 'stream_google'))
    combined = read_file_list(osp(base_dir, 'stream_combined'))

    yfcc = read_file_list(osp(base_dir, 'YFCC'))
    num_sample = 100 / (base_rate/100.) 

    #num_sample = int(math.ceil(len(inat) / (base_rate/100.)))
    num_sample = int(math.ceil(len(inat) * (100./base_rate -1)))
    write_data('stream_{}_{}_{:.2f}'.format(seed, 'inat', base_rate), [yfcc[:num_sample], inat], seed, base_dir=base_dir)
    num_sample = int(math.ceil(len(google) * (100./base_rate -1)))
    write_data('stream_{}_{}_{:.2f}'.format(seed, 'google', base_rate), [yfcc[:num_sample], google], seed, base_dir=base_dir)
    num_sample = int(math.ceil(len(combined) * (100./base_rate -1)))
    write_data('stream_{}_{}_{:.2f}'.format(seed, 'combined', base_rate), [yfcc[:num_sample], combined], seed, base_dir=base_dir)
   
def get_data_files(base_dir, seed=1404):
    inat = read_file_list(osp(base_dir, 'INAT'))
    google = read_file_list(osp(base_dir, 'GOOGLE'))
    yfcc_negative = read_file_list(osp(base_dir, 'TEST-YFCC')) #TODO Change
    combined = []
    combined.extend(inat)
    combined.extend(google)
    # Train files 
    train_inat, inat = sample_data(inat[:], 50, seed)
    train_google, google = sample_data(google[:], 50, seed)
    train_combined, combined = sample_data(combined[:], 50, seed)
    write_data('train_google', [train_google], seed, base_dir=base_dir)
    write_data('train_inat', [train_inat], seed, base_dir=base_dir)
    write_data('train_combined', [train_combined], seed, base_dir=base_dir)
    # test files
    test_inat, inat = sample_data(inat[:], 100, seed)
    test_google, google = sample_data(google[:], 100, seed)
    test_combined, combined = sample_data(combined[:], 100, seed)
    write_data('test_google', [yfcc_negative, test_google], seed, base_dir=base_dir)
    write_data('test_inat', [yfcc_negative, test_inat], seed, base_dir=base_dir)
    write_data('test_combined', [yfcc_negative, test_combined], seed, base_dir=base_dir)
    # stream
    write_data('stream_google', [google], seed, base_dir=base_dir)
    write_data('stream_inat', [inat], seed, base_dir=base_dir)
    write_data('stream_combined', [combined], seed, base_dir=base_dir)
