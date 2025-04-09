//
// Created by jayki on 4/6/2025.
// This file is a quick test of how memory is stored in a struct which that serves to answer my question of if I should put my segment table in the PCB or not.
// This does keep the memory contigous so it appears that by doing this we would be inline with what project 4 is asking us.
//

#include <iostream>
struct myStruct{
  int a;
  int b;
  int c;
  int d;

};

int main(){
  myStruct s;

  std::cout << "Address of s: " << &s << std::endl;
  std::cout << "Address of s.a: " << &s.a << std::endl;
  std::cout << "Address of s.b: " << &s.b << std::endl;
  std::cout << "Address of s.c: " << &s.c << std::endl;
  std::cout << "Address of s.d: " << &s.d << std::endl;

  return 0;
}
