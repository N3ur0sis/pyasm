# Erreurs sémantiques traitées et restant à traiter


## Erreurs sémantiques Gérées :

- Fonction redéfinies avec le même nom (quelque soit la signature)
- Fonction définie avec un nom réservé (built-in) : range, list, len, print
- Fonction utilisée sans être déclarée
- Fonction utilisées avec la bonne arité des paramètres
- Fonction built-in list, len et range utilisées avec un et un seul paramètre
- Fonction print : utilisation possible avec un nombre quelconque (>0) de paramètres

- shadowing : une variable de boucle ne peut avoir le même nom qu'une variable précédemment définie
- shadowing : une variable locale à une fonction ne doit pas avoir le même nom qu'un paramètre
- shadowing : une variable dans une boucle for ne doit pas avoir le même nom que la variable de boucle

- Vérifie que tout "return" se situe au sein d'une fonction et pas en dehors de toute fonction

## Erreurs sémantiques à traiter :

- détection du numéro de ligne dans l'analyse sémantique et dans la création des TDS
- unused parameter
- utilisation correcte des fonction built-in : range, len, list

## Définition de variable :

- une variable définie avec une certaine valeur et un certain type va simplement écraser la variable précédente
ex :
a =6
...
a = "Hello"

-> pas d'erreur, seulement une redef de a

## Erreurs sémantiques dont le traitement est particulier

- "utilisation d'une variable non déclarée" : 
lors de la génération de code, si on constate que la variable qu'on veut utiliser est de type "auto" dans la table des symboles (type auto correspondant à un type "unknown"), alors cela signifie que cette variable n'a jamais été initialisée avant l'utilisation, donc on soulèvera une erreur sémantique à ce moment. Si une variable est initialisée à un moment, alors son type est inféré et inscrit dans la table des symboles et donc lors de son utilisation, la génération de code va détecter un type précis et ne soulèvera pas d'erreur.

- "Utilisation d'une variable avec un type incorrect" : 
idem, lors de la génération de code
