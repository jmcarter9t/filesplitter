/** 
 * @file 
 * @author Jason M. Carter
 * @author Aaron E. Ferber
 * @date April 2017
 * @version
 */

#include "utilities.hpp"

const std::string string_utilities::DELIMITERS = " \f\n\r\t\v";

StrVector string_utilities::split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    split(s, delim, std::back_inserter(elems));
    return elems;
}
    
std::string& string_utilities::rstrip( std::string& s )
{
  return s.erase( s.find_last_not_of( string_utilities::DELIMITERS ) + 1 );
}

std::string& string_utilities::lstrip( std::string& s )
{
  return s.erase( 0, s.find_first_not_of( string_utilities::DELIMITERS ) );
}

std::string& string_utilities::strip( std::string& s )
{
  return string_utilities::lstrip( rstrip ( s ));
}
