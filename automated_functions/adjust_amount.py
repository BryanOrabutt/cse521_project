"""
Automatically adjust the amount of food based on the leftover weight

TODO:
1, a meaningful way to add ml to it.

"""

import numpy as np
import scipy as sp


amount = 10
weight = 0 # which should be changed to reading from sensors


if weight > 0 :
    next_amount = 10 - weight
else :
    if :