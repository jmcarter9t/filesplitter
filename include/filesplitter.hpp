#pragma once
 
#ifndef FILESPLITTER_HPP
#define FILESPLITTER_HPP

#include <mutex> 
#include <cstdio>
#include "tool.hpp"
#include "spdlog/spdlog.h"

/**
 * @brief predicate indicating whether a file exists on the filesystem.
 *
 * @param fn the file name and path to check the existance of.
 *
 * @return true if it exists, false otherwise.
 */
bool fileExists( const std::string& fn );

/**
 * @brief predicate indicating whether a path/directory exists on the filesystem.
 *
 * @param dn the directory path to check the existance of.
 *
 * @return true if it exists, false otherwise.
 */
bool dirExists( const std::string& dn );

/**
 * A class that performs multithreaded file split operations.
 */
class FileSplitter : public tool::Tool {
    public:
        using FSPtr = std::shared_ptr<FileSplitter>;
        using LogPtr = std::shared_ptr<spdlog::logger>;

        static char rdelim;
        static char fdelim;

        /**
         * @brief Construct a FileSplitter instance.
         *
         * @param name the name of the tool
         * @param description a description for the tool
         */
        FileSplitter(const std::string& name, const std::string& description);

        /**
         * @brief Get the full path and name to the input file being split.
         *
         * @return the full path and name as a string.
         */
        const std::string& getInputFileName() const;

        /**
         * @brief Get the full path to the output directory where split files will be written.
         *
         * @return the full path and name as a string.
         */
        const std::string& getOutputDirectoryName() const;

        /**
         * @brief Get the size of the input file in bytes.
         *
         * @return the size of the file in bytes.
         */
        long getFileSize() const;

        /**
         * @brief If the input file has a header this will produce that line INCLUDING the newline character.
         *
         * @return the header including newline; empty string if no header exists.
         */
        const std::string& getHeader() const;

        /**
         * @brief a predicate that indicates whether the input file had a header.
         *
         * @return true if the input file has a header; false otherwise.
         */
        bool hasHeader() const;

        /**
         * @brief Initialize a multithreaded logger for the filesplitter.
         *
         * @param logname the name of the log file.
         * @param path where the log file will be written, relative or absolute.
         *
         * @return true on success; false otherwise.
         */
        bool initLogger( std::string& logname, std::string& path );

        int operator() ( void ) override;

        /**
         * Splits the file.
         *
         * @param filename the file to split.
         *
         * @return the program exit status.
         */
        int splitFile( void );

    private:
        std::string ifname_;                                     ///> the name of the file to split.
        std::string odname_;                                     ///> the directory for the split files.
        long ifsize_;                                            ///> the size in bytes of the file.
        std::string header_;                                     ///> the header line of the file or the empty string if no header.
        LogPtr logger_;                                          ///> multithreaded logger.
        std::vector<uint32_t> keylist_;                          ///> Contains the indices of the colums to use as keys.

        bool initOutputDirectory( std::string& odname );
        long initInputFile( std::string& ifname, std::string& header );
};

/**
 * Functor for finding and writing blocks in a thread.
 * Good link on copy options: http://stackoverflow.com/questions/10195343/copy-a-file-in-a-sane-safe-and-efficient-way
 */
class BlockHandler {
    public:
        static constexpr int BUFSIZE = 8 * 1024;                ///> 8k seems a good buffer size.
        
        /**
         * @param ifname the name of the file.
         * @param ifsize the total size of the file; this includes HEADER.
         * @param header header to append to all blocks; could be the empty string.
         * @param begin byte offset of the "proposed" beginning of the data portion of the block (will never include header).
         * @param end byte offset of the "proposed" end of the block; could be the last byte in the file.
         */
        BlockHandler( const std::string& ifname, const std::string& odname, long ifsize, const std::string& header, FileSplitter::LogPtr logger, const std::vector<uint32_t>& keylist );

        /**
         * @brief Destroy the block handler.
         */
        ~BlockHandler( void );

        /**
         * @brief Execute the thread to handle a file block.
         *
         * Strategy to identify boundaries of focus:
         *
         * 1. Both begin and end positions move forward during the search unless they cannot.
         *    a. begin position cannot move forward by definition.
         *    b. end position is "sticky" and cannot move forward by algorithm.
         * 2. When a block is homogeneous (same key found at beginning and end) and end is not EOF, the thread will exit.
         * 3. search for record offset only looks forward; therefore, when searching from EOF it will find the key on
         * the last line.
         *
         * Once BOUNDS are found, then we can partition into blocks.
         *
         * 1. If begin key and end key are the same, then write the entire block.
         * 2. current key is the end key.
         * 3. epos = end; bpos = begin (do not modify begin and end yet...)
         * 4. Jump toward the front: jp = floor((bpos+epos)/2)
         * 5. While jp >= bpos && jp <= epos
         * 6. Get key.
         * 7. If the same as end, 
         *    a. update epos = jp
         *    b. jump toward front: jp = floor((bpos+jp)/2); goto 4.
         * 8. If different, 
         *    a. update bpos = jp
         *    b. jump toward the end: jp = floor((jp+epos)/2); goto 5. 
         *
         * Expected Results for Special Cases:
         *
         * 1. When file is entirely of one key, the thread with end == EOF will write a copy of the entire file; all
         * other threads will exit doing nothing.
         * 2. All homogeneous blocks will do nothing
         */
        void operator()( long begin, long end );
        
        /**
         * @brief Set the position in a file (inf), to the beginning of the current record and return the byte offset of that
         * position.
         *
         * If start <= begin, the file will be positioned at start (this is an abnormal parameter setting).
         * If start or begin is less than 0 it will be set to 0.
         * If start is greater than the size of the file (or within the last record), the start of the last record will be
         * returned.
         *
         * @param inf the file pointer to set; it is expected to be open.
         * @param soff the position in the file from which to start the search; start should be >= 0.
         * @return the byte offset of the start of the record the parameter was within, or -1 on error.
         */
        long setRecordStartOffset( FILE* inf, long soff );

        /**
         * @brief Extract the record key from the record designated by spos return it along with the byte offset of the first
         * byte in that key.
         *
         * @param f the file to identify the key within.
         * @param soff the starting search byte offset within f; this can be anywhere within the record of interest.
         * @param key location to set the key (this is modified by the method).
         * @return long the byte offset of the first character in key (the beginning of the record).
         */
        long setRecordKey( FILE* f, long soff, std::string& key );
        long setRecordMultiKey( FILE* f, long soff, std::string& key );

        /**
         * @brief Return the byte offset of the first record in f having the same key as the record that includes the byte at
         * soff. In other words, soff can be anywhere in a record (start, ending \n, etc).
         *
         * Steps:
         *
         * 1. Find the key of the initial record.
         * 2. Perform a logarithmic search, i.e., jump to half-way points forward and backward for the first instance of that
         * key moving forward in the file.
         * 3. Return the offset of the first byte of that first record.
         *
         * @param f the file to search.
         * @param soff the byte offset in f to start and identify the key to search for.
         * @param begin the absolute beginning byte offset in f.
         * @param end the absoute end byte offset in f.
         *
         * @return the byte offset of the first record in f having the required key.
         * @note bkey_ will contain the key for the first record (it is private)
         */
        long findFirstRecord( FILE* f, long soff, long end );

        /**
         * NOTE: This is faster than using c++ streams.  Not by much, but the code is almost the same when you have to slice
         * out of the file.
         */
        long transfer( long soff, long bytes_to_write, const std::string& ofn );

    private:
        const std::string& ifname_;
        const std::string& odname_;
        long ifsize_;
        const std::string& header_;
        FileSplitter::LogPtr logger_;
        const std::vector<uint32_t>& keylist_;
        std::string bkey_;
        std::string ckey_;
        char buf[BUFSIZE];                                      ///> one buffer per handler.
};

#endif
