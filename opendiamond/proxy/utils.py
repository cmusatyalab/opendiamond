from zipfile import ZipFile
from collections import defaultdict

def load_dataset_from_zipfile(zf):
    """ 
    Assume file structure in zip file is like:
        label1/111.jpg
        label1/a24.jpg
        ...
        label2/34b.jpg
        label2/4333.jpg.
    Return: dict(label -> list(file bytes))
    """
    assert isinstance(zf, ZipFile)
    dataset = defaultdict(list)
    label_list = []
    for path in zf.namelist():
        if not path.endswith('/'):
            label = path.split('/')[0].lower()
            dataset[label].append(zf.read(path))
            if label not in label_list:
                label_list.append(label)
    assert 'negative' in label_list
    label_list.pop(label_list.index('negative'))
    label_list = ['negative'] + label_list
    return label_list, dataset

