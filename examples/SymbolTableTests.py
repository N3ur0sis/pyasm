def function1(param1, param2, z):
    print("hehe")
    x = 7
    return(8)

def function2():
    print("hello")
    y = 8
    return(9)

def function3(parameter1, param2, param3, x):
    print("entrée en fonction 3")
    z = x * y + 6                       #Est-ce que y est bien le y global qui est utilisé ? Et z, cest lequel ? (normalement z local-->PB, y global, x param)
    variable0Function3 = "this is a string"
    variable1Function3 = [True, False, True]
    if x <= 9:
        print("Hello IF")
        y = 4 * 6                       #Est-ce que le y utilisé est le y global ?
        return y
    else:
        variable1Function3 = 8
        for i in range(list(x)):
            z = 19
            if z > x:
                z = z + i * variable1Function3
            else:
                variable2Function3 = 90
                z = z - i * variable2Function3
                y = function2()
        
        return z % variable2Function3

def function4(parameter):
    variableString = "This function is not used so it is not visible in the symbol tables"


variable = 10
variable2 = "hello"
variable3 = 12

y = [5,a,"hello", 7 * 9, 8 + variable, 9, 10]
x = 5
z = variable
t = 5 * 8
u = variable2 * variable3
v = True
w = False
a = function1(variable, variable2, u)

for i in range(5):
    variableDansFor1 = 7
    variableDansFor2 = "string"
    if i == 3:
        variableDansForIF3 = 9
        variableDansForIF4 = True
        for j in range([1,2,3,4,5]):
            variableDansForIF5 = 7
            variableDansForIF6 = False
            if j == 2:
                variableDansForIF7 = 9
                variableDansForIF8 = True
                for k in range(5):
                    variableDansForIF9 = 7
                    variableDansForIF10 = False
                    if k == 2:
                        variableDansForIF11 = 9
                        variableDansForIF12 = True
            else :
                variableDansForELSE1 = 19
                variableDansForELSE2 = False
    else:
        variableDansForELSE3 = 19

i = "I am now a global variable"    #Cette variable est globale mais définie après la boucle for précédente : il n'y a pas de problème de shadowing
l = "I am also a global variable"   #Cette variable est globale et il y a un phénomène de shadowing qui suit : une erreur doit être soulevée

for l in range(list(5)):
    print("La variable l subit du shadowing")

if x <= y:
    z = 7
    superVariableSeulementDansIF = 9
    v = [True, False, True]
else:
    superVariableSeulementDansELSE = 19
    returnOfFunction3 = function3(u, v, w, t)

