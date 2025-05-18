#include <queue>
#include <string>
#include <iostream>
#include "errorManager.h"
#include <unordered_set>

// Définitions de couleurs ANSI
#define RESET "\033[0m"
#define RED "\033[31m"
#define BLUE "\033[34m"
#define YELLOW "\033[33m"
#define GREEN "\033[32m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"
#define UNDERLINE "\033[4m"
#define MAGENTA "\033[35m"

void ErrorManager::addError(const Error& error) {
    errorQueue.push(error);
}

void ErrorManager::displayErrors() const {
    if (errorQueue.empty()) {
        std::cout << GREEN << "\n" << "No errors to display." << RESET << std::endl;
        return;
    }

    std::cout << BOLD << UNDERLINE << "\nErrors found:\n" << RESET << std::endl;

    std::unordered_set<int> lines;
    std::queue<Error> tempQueue = errorQueue;
    while (!tempQueue.empty()) {
        const Error& error = tempQueue.front();

        // Sélection de la couleur en fonction du type d'erreur
        std::string errorColor;
        if (error.type == "Syntax") {
            errorColor = RED; // Rouge pour les erreurs Syntaxiques
            if (lines.find(error.line) != lines.end()) {
                tempQueue.pop();
                continue;
            }
            lines.insert(error.line);
        } else if (error.type == "Lexical") {
            errorColor = BLUE; // Bleu pour les erreurs Lexicales
        } else if (error.type == "Semantic") {
            errorColor = GREEN; // Vert pour les erreurs Sémantiques
        } else if (error.type == "Semantics") {
            errorColor = MAGENTA; // Magenta pour les erreurs sémantiques issues de la génération de code
        } else {
            errorColor = CYAN; // Cyan par défaut pour d'autres types (erreurs sémantiques de la création de la TDS)
        }

        // Affichage coloré des erreurs
        std::cout << errorColor << "[" << error.type << " error]" << RESET
                  << " " << YELLOW << "Line " << error.line << RESET
                  << ": " << error.message 
                  << error.value << std::endl;
        tempQueue.pop();

        std::cout << "----------------------------------------------------------------------------------------------------------------------------" 
                  << std::endl;
    }
}

bool ErrorManager::hasErrors() const {
    return !errorQueue.empty();
}
