# -*- coding: utf-8 -*-

import os
from uuid import uuid4


# Constants
CURR_PATH = os.getcwd()
YEARS = 10


def fib(num: int) -> int:
    """ Calculate fibonacci num """

    if num == -1: return 0
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
        self.path = f'{branch}/leave_{self.created_at}_{uuid4().hex}'
        touch(self.path)


class Branch:
    def __init__(self, created_at: int, parent: str, mode: str) -> None:
        # Save created year
        self.created_at: int = created_at

        self.path: str = f'{parent}/branch_{self.created_at}_{uuid4().hex}'

        if mode == 'birch':
            self.num_of_leaves: int = 10
        else:
            self.num_of_leaves: int = 100

        self.leaves: list = []

        self.max_sub_branches_for_leaves: int = 5
        mkdir(self.path)

    @property
    def num_sub_branches(self) -> int:
        return len([d[0] for d in os.walk(self.path)]) - 1

    def is_old(self, max_year: int, current_year: int) -> bool:
        """ Method that get information about branch's age """
        if current_year - self.created_at < max_year:
            return True
        return False

    def grow_leaves(self, year: int) -> None:
        if self.num_sub_branches <= self.max_sub_branches_for_leaves:
            self.leaves: list = [
                Leave(year, self) for num in range(self.num_of_leaves)
            ]

    def __str__(self) -> str:
        return self.path


class Tree:
    def __init__(self, years: int, mode: str) -> None:
        self.years: int = years
        self.branches: list = []

        self.mode: str = mode
        mkdir(self.mode)
    
    def grow(self) -> None:
        if self.mode == 'birch':
            leaves_years_max: int = 3
        if self.mode == 'spruce':
            leaves_years_max: int = 1
        
        for year in range(self.years):
            self.branches.append(Branch(year, self.mode, self.mode))
            for branch in self.branches:
                if not branch.is_old(leaves_years_max, year):
                    for num in range(fib(year - 1)):
                        self.branches.append(Branch(year, branch, self.mode))
                    branch.grow_leaves(year)

    
birch: Tree = Tree(YEARS, 'birch')
birch.grow()

spruce: Tree = Tree(YEARS, 'spruce')
spruce.grow()
