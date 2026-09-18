#!/bin/python
# coding=utf8

import roomai.games.common

logger = roomai.get_logger()


class AbstractAction(object):
    '''
    The abstract class of an action. 
    '''

    def __init__(self, key):
        if not isinstance(key,str):
            raise TypeError("The key for Action is an str, not %s"%(type(str)))

        self.__key__ = key

    def __get_key__(self):
        return self.__key__

    key = property(__get_key__, doc="The key of the action. Every action in RoomAI has a key as its identification."
                                    " We strongly recommend you to use the lookup function to get an action with the specified key")

    @classmethod
    def lookup(self, key):
        '''
        Get an action with the specified key. 
        We strongly recommend you to use the lookup function to get an action with the specified key, rather than use the constructor function.

        :param key: the specified key
        :return:  the action with the specified key
        '''
        raise NotImplementedError("Not implemented")

    def __deepcopy__(self, memodict={}, newinstance=None):
        if newinstance is None:
            newinstance = AbstractAction()
        newinstance.__key__ = self.__key__
        return newinstance