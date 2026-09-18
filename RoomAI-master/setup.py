#!/bin/python

from setuptools import setup

setup(  name        = "roomai",
        version     = "0.1.16",
        description = "A toolkit for developing and comparing imperfect information game bots",
        url         = "https://github.com/roomai/RoomAI",
        author      = "RoomAI Dev",
        author_email= "lili1987mail@gmail.com",
        license     = "MIT",
        packages    = ["roomai_models","roomai","roomai.doudizhupoker","roomai.kuhnpoker","roomai.common","roomai.texasholdem","roomai.fivecardstud","roomai.sevenking","roomai.bridge"],
        zip_safe    = False)
