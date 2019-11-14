#
#  The OpenDiamond Platform for Interactive Search
#
#  Copyright (c) 2011-2019 Carnegie Mellon University
#  All rights reserved.
#
#  This software is distributed under the terms of the Eclipse Public
#  License, Version 1.0 which can be found in the file named LICENSE.
#  ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS SOFTWARE CONSTITUTES
#  RECIPIENT'S ACCEPTANCE OF THIS AGREEMENT
#

import sys
import json
import numpy as np
import random
from sklearn.svm import SVC
from sklearn.metrics import f1_score
from scipy.stats.stats import pearsonr

class Item(object):
    def __init__(self, label, name, feature):
        self.label = label
        self.name = name
        self.feature = feature

class ItemList(object):
    def __init__(self):
        self._names = []
        self._features = []
        #Can associate each item with a weight

    def add(self, item):

        name = item.name
        feature = item.feature

        if name is None:
            self._features.append(feature)
            return

        if name in self._names:
            index = self._names.index(name)
            self._features[index] = feature
        else:
            self._names.append(name)
            self._features.append(feature)
        return

    def sample(self, num=sys.maxsize):
        num = min(num, self.size())
        return random.sample(self._features, num)

    def size(self):
        return len(self._features)


class ProxyFilter(object):

    def __init__(self, proxy_filter):
        self.filter = proxy_filter
        self.blob = proxy_filter.blob
        self.code = proxy_filter.code
        self.max_models = int(proxy_filter.arguments[1])
        self.majority_frac = float(proxy_filter.arguments[2])
        self.threshold = float(proxy_filter.arguments[3])
        self.positive_items = ItemList()
        self.negative_items = ItemList()
        self.init_negative_items = ItemList()
        self.full_models = []
        self.current_max = 0
        self.num_model_per_iter = max(2, int(self.max_models/3.))

    def addItem(self, label, name, feature):
        item = Item(label, name, feature)
        if label == 1:
            self.positive_items.add(item)
        else:
            if name is None:
                self.init_negative_items.add(item)
            else:
                self.negative_items.add(item)
        return

    def addItemList(self, items):
        len_items = len(items.names)
        for i in range(len_items):
            label = items.labels[i]
            name = items.names[i]
            item = Item(label, name, json.loads(items.features[i]))
            if label:
                self.positive_items.add(item)
            else:
                self.negative_items.add(item)
        return

    def trainEnsemble(self):
        C_range = np.logspace(-7,7,15,base=2)
        num_positive = self.positive_items.size()
        num_negative = self.init_negative_items.size() + self.negative_items.size()
        num_sample = min(num_positive, num_negative)

        for j in range(self.num_model_per_iter):
            #model = {}
            #model['id'] = self.current_max + j
            #model['model'] = SVC(C=random.choice(C_range), gamma='scale',
            #                    probability=True, class_weight='balanced')
            #Choose negative samples 70% from labeled rest from init
            model = SVC(C=random.choice(C_range), gamma='scale',
                                probability=True, class_weight='balanced')
            num_left = num_sample
            X_neg = []
            if self.negative_items.size():
                X_neg = self.negative_items.sample(int(0.7 * num_sample))
                num_left -= len(X_neg)
            X_neg += self.init_negative_items.sample(num_left)
            num_negative = len(X_neg)
            m_train = np.array(self.positive_items.sample() + X_neg)
            m_label = np.array([1]*num_positive + [0]*num_negative)
            model.fit(m_train, m_label)
            self.full_models.append(model)

        #self.curr_num += self.num_model_per_iter
        print("After train Length of models:{} \n#Example: Neg(Init+New)XPos: ({}+{})X{}"
            .format(len(self.full_models), self.init_negative_items.size(),
            self.negative_items.size(), num_positive))
        return


    def sortModels(self):
        f1_scores = []
        m_test = (self.positive_items.sample() +
                self.negative_items.sample() +
                self.init_negative_items.sample())
        y_label = np.array([1] * self.positive_items.size() +
                    [0] * (self.negative_items.size() + self.init_negative_items.size()))
        f1_score_ = []
        predictions = []
        for m in self.full_models:
            pred = m.predict(m_test)
            predictions.append(pred)
            f1_score_.append(f1_score(y_label, pred))

        ordered_model = np.argsort(-1*np.array(f1_score_))
        self.full_models = (np.array(self.full_models)[ordered_model]).tolist()
        predictions = (np.array(predictions)[ordered_model]).tolist()
        self.full_models = self.full_models[:self.max_models]
        predictions = predictions[:self.max_models]
        return predictions

    def getPrunedModels(self):
        pruned_model = None
        predictions = self.sortModels()
        len_models = len(predictions)

        selected_models = [0, 1]
        for i in range(3, len_models):
            choose = 1
            for j in selected_models:
                corr, _ = pearsonr(predictions[j], predictions[i])
                if corr > 0.8:
                    choose = 0
                    break
            if choose:
                selected_models.append(i)

        new_models = (np.array(self.full_models)[selected_models]).tolist()
        print("#Models:{} After pruning #Models:{}".format(len_models, len(new_models)))
        return new_models





