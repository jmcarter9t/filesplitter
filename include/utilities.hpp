#pragma once

/** 
 * @file 
 * @author Jason M. Carter
 * @author Aaron E. Ferber
 * @date April 2017
 * @version
 */

#ifndef STRING_UTILITIES_H
#define STRING_UTILITIES_H

#include <string>
#include <sstream>
#include <iterator>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

using StrVector     = std::vector<std::string>;                             ///< Alias for a vector of strings.

namespace string_utilities {

extern const std::string DELIMITERS;

/**
 * @brief Split the provided string at every occurrence of delim and put the results in the templated type T.
 *
 * @param s the string to split.
 * @param delim the char where the splits are to be performed.
 * @param T the type where the string components are put.
 */
template<typename T>
    void split(const std::string &s, char delim, T result) {
        std::stringstream ss;
        ss.str(s);
        std::string item;
        while (std::getline(ss, item, delim)) {
            *(result++) = item;
        }
    }

/**
 * @brief Split the provided string at every occurrence of delim and return the components in a vector of strings.
 *
 * @param s the string to split.
 * @param delim the char where the splits are to be performed; default is ','
 * @return a vector of strings.
 */
StrVector split(const std::string &s, char delim = ',');


/**
 * @brief Remove the whitespace from the right side of the string; this is done
 * in-place; no copy happens.
 *
 * @param s the string to trim.
 * @return the new string without the whitespace on the right side.
 */
std::string& rstrip( std::string& s );

/**
 * @brief Remove the whitespace from the left side of the string to the first
 * non-whitespace character; this is done in-place; no copy happens.
 *
 * @param s the string to trim.
 * @return the new string without the whitespace on the left side.
 */
std::string& lstrip( std::string& s );

/**
 * @brief Remove the whitespace surrounding the string; this is done in-place; no copy
 * happens.
 *
 * @param s the string to trim.
 * @return the new string with the whitespace before and after removed.
 */
std::string& strip( std::string& s );

}  // end namespace.

#endif
