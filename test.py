def is_valid(grid, pos, num):
    row = pos // 9
    col = pos % 9
    for i in range(9):
        if grid[row * 9 + i] == num:
            return False
    for i in range(9):
        if grid[i * 9 + col] == num:
            return False
    box_x = col // 3
    box_y = row // 3
    for i in range(3):
        for j in range(3):
            idx = (box_y * 3 + i) * 9 + (box_x * 3 + j)
            if grid[idx] == num:
                return False
    return True

def find_empty(grid):
    for i in range(81):
        if grid[i] == 0:
            return i
    return -1

def solve(Liste):
    
    n = len(Liste)
    grid = list(range(n))
    k = 0
    for e in Liste:
        grid[k] = e
        k = k+1
    empty = find_empty(grid)
    if empty == -1:
        return True  
    for num in range(9):
        num = num+1
        if is_valid(grid, empty, num):
            grid[empty] = num
            if solve(grid):
                return True
            grid[empty] = 0

    return False

# Exemple de grille avec quelques zéros
sudoku = [5, 3, 0, 0, 7, 0, 0, 0, 0,6, 0, 0, 1, 9, 5, 0, 0, 0,0, 9, 8, 0, 0, 0, 0, 6, 0,8, 0, 0, 0, 6, 0, 0, 0, 3,4, 0, 0, 8, 0, 3, 0, 0, 1,7, 0, 0, 0, 2, 0, 0, 0, 6,0, 6, 0, 0, 0, 0, 2, 8, 0,0, 0, 0, 4, 1, 9, 0, 0, 5,0, 0, 0, 0, 8, 0, 0, 7, 9]
if solve(sudoku):
    print("Sudoku résolu :")
    for i in range(9):
        temp = list(range(9))
        for j in range(9):
            temp[j] = sudoku[i*9+j]
        print(temp)
else:
    print("Pas de solution trouvée.")
