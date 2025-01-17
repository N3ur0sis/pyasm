#include "errorManager.h"

void ErrorManager::addError(const std::string& errorMessage) {
    errorQueue.push(errorMessage);
}

void ErrorManager::displayErrors() const {
    if (errorQueue.empty()) {
        std::cout << "No errors to display." << std::endl;
        return;
    }

    std::queue<std::string> tempQueue = errorQueue; // Copie pour éviter de modifier la file originale
    while (!tempQueue.empty()) {
        std::cout << "Error: " << tempQueue.front() << std::endl;
        tempQueue.pop();
    }
}

bool ErrorManager::hasErrors() const {
    return !errorQueue.empty();
}
