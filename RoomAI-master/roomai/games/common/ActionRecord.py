#!/bin/python

class ActionRecord(object):
    def __init__(self, playerid, action):
        self.__playerid__ = playerid
        self.__action__   = action

    def __get_playerid__(self):
        return self.__playerid__
    playerid = property(__get_playerid__)

    def __get_action__(self):
        return self.__action__
    action = property(__get_action__)
