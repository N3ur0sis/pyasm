#pragma once
#include <string>
#include <queue>
#include <iostream>

struct Error {
    std::string message;
    std::string value = "";
    std::string type;
    int line;      
};

class ErrorManager {
public:
    // Ajouter une erreur sous forme d'objet Error
    void addError(const Error& error);

    // Afficher tous les messages d'erreur
    void displayErrors() const;

    // Vérifier si des erreurs existent
    bool hasErrors() const;

private:
    std::queue<Error> errorQueue; // File pour stocker les objets Error
};
