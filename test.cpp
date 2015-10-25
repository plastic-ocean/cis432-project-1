#include "test.h"


std::vector<std::string> SplitString(std::string input) {
  std::vector<std::string> result;
  std::string word = "";

  for (char c : input) {
    if (c != ' ') {
      word += c;
    } else {
      result.push_back(word);
      word = "";
    }
  }
  result.push_back(word);

  return result;
}


int main() {
//  std::string test = "this is a test";
//
//  std::vector<std::string> result = SplitString(test);

  srand((unsigned int) time(NULL));
  int client_port = (rand() % 1000) + 4000;

  std::cout << client_port << std::endl;

//  for (auto word : result) {
//    std::cout << word << std::endl;
//  }
}