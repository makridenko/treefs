# -*- coding: utf-8 -*-

import os
from uuid import uuid4


# Constants
CURR_PATH = os.getcwd()
YEARS = 10


def fib(num: int) -> int:
    """ Calculate fibonacci num """

    if num == 0: return 0
    if num == 1: return 1
    return fib(num - 1) + fib(num - 2)


def touch(filename: str) -> None:
    """ Implements touch method """

    # Using append mode
    open(f'{CURR_PATH}/{filename}', 'a')


def mkdir(dirname: str) -> None:
    """ Implements mkdir method """
    os.mkdir(f'{CURR_PATH}/{dirname}')


class Leave:
    def __init__(self, created_at: int, branch: str) -> None:
        self.created_at = created_at
        self.path = f'{branch}/leave_{self.created_at}_{uuid4()}'
        touch(self.path)


class Branch:
    def __init__(self, created_at: int, parent: str) -> None:
        # Save created year
        self.created_at = created_at
        self.path = f'{parent}/branch_{self.created_at}_{uuid4()}'
        self.num_of_leaves = 10
        self.leaves = []
        self.max_sub_branches_for_leaves = 5
        mkdir(self.path)

    @property
    def num_sub_branches(self):
        return len([d[0] for d in os.walk(self.path)]) - 1

    def is_one_year_old(self, current_year: int):
        """ Method that get information about branch's age """
        if current_year == self.created_at + 1:
            return True
        return False

    def grow_leaves(self, year: int) -> None:
        if self.num_sub_branches <= self.max_sub_branches_for_leaves:
            self.leaves = [
                Leave(year, self) for num in range(self.num_of_leaves)
            ]

    def __str__(self):
        return self.path


class Tree:
    def __init__(self, years: int) -> None:
        mkdir('birch')
        self.years = years
        self.branches = []

    def grow(self):
        for year in range(self.years):
            self.branches.append(Branch(year, 'birch'))
            for branch in self.branches:
                if branch.is_one_year_old(year):
                    for num in range(fib(year - 1)):
                        self.branches.append(Branch(year, branch))
                    branch.grow_leaves(year)


tree = Tree(YEARS)
tree.grow()
