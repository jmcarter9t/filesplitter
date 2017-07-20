#include "filesplitter.hpp"
#include "utilities.hpp"
#include <sstream>
#include <cmath>
#include <thread>

// for both windows and linux.
#include <sys/types.h>
#include <sys/stat.h>

bool fileExists( const std::string& fn ) {
    static struct stat info;

    if (stat( fn.c_str(), &info) != 0) {     // bad stat; doesn't exist.
        return false;
    } else if (info.st_mode & S_IFREG) {    // exists and is a regular file.
        return true;
    } 

    return false;
}

bool dirExists( const std::string& dn ) {
    static struct stat info;

    if (stat( dn.c_str(), &info) != 0) {     // bad stat; doesn't exist.
        return false;
    } else if (info.st_mode & S_IFDIR) {    // exists and is a directory.
        return true;
    } 

    return false;
}

char FileSplitter::rdelim = '\n';
char FileSplitter::fdelim = ',';

FileSplitter::FileSplitter(const std::string& name, const std::string& description ) :
    tool::Tool( name, description, true ),
    ifname_{ "splitfile" },
    odname_{ "output" },
    ifsize_{ 0 },
    header_{},
    logger_{},
    keylist_{}
{
}

bool FileSplitter::initLogger( std::string& logname, std::string& path )
{
    const static std::string fnname{"initLogger"};

    std::stringstream header_ss;
    char c;

    if ( logger_ != nullptr ) return true;

    if (!dirExists( path )) {
#ifndef _MSC_VER
        if (mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) != 0)   // linux
#elif _MSC_VER 
        if (_mkdir(path.c_str()) != 0)                                          // windows
#endif
        {
            std::cerr << "Error making the logging directory.\n";
            return false;
        }
    }
    
    logname = path + logname;

    if ( fileExists( logname ) ) {
        if ( std::remove( logname.c_str() ) != 0 ) {
            std::cerr << "Error removing the previous information log file.\n";
            return false;
        }
    }

    // setup information logger.
    logger_ = spdlog::basic_logger_mt("log", logname);
    // put thread first so we can sort...
    logger_->set_pattern("%t [%H:%M:%S.%f] (%l) %v");
    logger_->set_level( spdlog::level::trace );

    if ( optIsSet('v') ) {
        if ( "trace" == optString('v') ) {
            logger_->set_level( spdlog::level::trace );
        } else if ( "debug" == optString('v') ) {
            logger_->set_level( spdlog::level::debug );
        } else if ( "info" == optString('v') ) {
            logger_->set_level( spdlog::level::info );
        } else if ( "warning" == optString('v') ) {
            logger_->set_level( spdlog::level::warn );
        } else if ( "error" == optString('v') ) {
            logger_->set_level( spdlog::level::err );
        } else if ( "critical" == optString('v') ) {
            logger_->set_level( spdlog::level::critical );
        } else if ( "off" == optString('v') ) {
            logger_->set_level( spdlog::level::off );
        } else {
            logger_->warn("{} information logger_ level was configured but unreadable; using default.", fnname);
        }
    }

    return true;
}

bool FileSplitter::initOutputDirectory( std::string& odname )
{
    const static std::string fnname{ "initOutputDirectory" };

    if ( odname.back() != '/' ) {
        odname += '/';
    }

    if (!dirExists( odname )) {
#ifndef _MSC_VER
        if (mkdir( odname.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH ) != 0)   // linux
#elif _MSC_VER 
        if (_mkdir( odname.c_str()) != 0)                                          // windows
#endif
        {
            logger_->error("{} unable to create the output directory: {} ... halting.", fnname, odname);
            return false;
        }
    }
    return true;
}

long FileSplitter::initInputFile( std::string& ifname, std::string& header )
{
    static std::string fnname{"initInputFile"};

    char c;
    struct stat finfo;
    FILE* inf = fopen( ifname.c_str(), "rb" );

    if ( !inf || (fstat( fileno(inf), &finfo ) != 0) ) {
        std::cerr << "Cannot open the input file named: " << ifname << '\n';
        logger_->error("{} Cannot stat file: {}.", fnname, ifname );
        return -1;
    }

    // set this to the empty string.
    header.clear();

    if ( finfo.st_size > 0 ) {
        logger_->info("{} Input file {} has {} bytes.", fnname, ifname, finfo.st_size);

        if (optIsSet('H')) {
            // ensure we are at the beginning of the file.
            if ( fseek( inf, 0, SEEK_SET ) != 0 ) {
                logger_->error("{} fseek to start of file to get header failed.", fnname);
                return -1;
            }

            while ( ( c = fgetc(inf) ) != EOF ) {
                // add the newline to the header string.
                header.push_back(c);
                if ( c == rdelim ) break;
            }
        } 
    }

    fclose( inf );
    return finfo.st_size;
}

const std::string& FileSplitter::getInputFileName( void ) const
{
    return ifname_;
}

const std::string& FileSplitter::getOutputDirectoryName( void ) const
{
    return odname_;
}

const std::string& FileSplitter::getHeader( void ) const
{
    return header_;
}

long FileSplitter::getFileSize( void ) const
{
    return ifsize_;
}

/**
 * Runner function.
 */
int FileSplitter::splitFile( void ) {
    static std::string fnname{"splitFile"};

    std::string path{"logs/"};
    std::string logname{"filesplitter.log"};

    if (optIsSet('L')) {
        // replace default
        path = getOption('L').argument();
    }

    if ( path.back() != '/' ) {
        path += '/';
    }

    initLogger( logname, path );

    if (!hasOperands()) {
        logger_->error("{} must have an input file... halting!", fnname);
        return EXIT_FAILURE;
    }

    ifname_ = operands[0];
    ifsize_ = initInputFile( ifname_, header_ );
    if ( ifsize_ <= 0 ) {
        logger_->error("{} The input file: {} size returned as {}; empty or an error with stat().", fnname, ifname_, ifsize_);
        return EXIT_FAILURE;
    }

    if ( optIsSet('o') ) {
        odname_ = getOption('o').argument();
    } // else use default.

    if ( !initOutputDirectory( odname_ ) ) return EXIT_FAILURE;

    int threads = std::thread::hardware_concurrency();

    if ( optIsSet('t') ) {
        try {
            threads = optInt('t');
        } catch ( std::exception& e ) {
            // stick with default.
        }
    }

    if ( optIsSet('k') ) {
        std::string key_arg = getOption('k').argument();
        StrVector keys = string_utilities::split( key_arg );  // uses ',' as the delim.
        for ( std::string& k : keys ) {
            try {
                keylist_.push_back( static_cast<uint32_t>( std::stoi( k ) ));
            } catch (std::exception& e) {
                // pass and keep going.
            }
        }
    }

    if ( keylist_.empty() ) {
        // default to use the first column as the key.
        keylist_.push_back( 1 );
    } else {
        // sort the keylist_ to make sure it is from smallest to largest.
        std::sort( keylist_.begin(), keylist_.end() );
    }

    std::vector<std::thread> thread_list;
    long block_size = std::ceil(static_cast<double>(ifsize_ - header_.length())/static_cast<double>(threads));

    // initiate all the large block handler threads.
    // starting offset will jump over the header.
    for ( long b = header_.length(); b < ifsize_; b += block_size ) {
        BlockHandler bh{ ifname_, odname_, ifsize_, header_, logger_, keylist_ };

        // call BlockHandler functor with two arguments: the block bounds, then throw it in the list.
        thread_list.emplace_back( std::thread{ std::move(bh), b, b+block_size } );
    }

    // join all the threads back to the main thread.
    for ( auto& t : thread_list ) {
        t.join();
    }

    return EXIT_SUCCESS;
}

int FileSplitter::operator()( void )
{
    return splitFile();
}

BlockHandler::BlockHandler( const std::string& ifname, const std::string& odname, long ifsize, const std::string& header, FileSplitter::LogPtr logger, const std::vector<uint32_t>& keylist ) :
    ifname_{ ifname },
    odname_{ odname },
    ifsize_{ ifsize },
    header_{ header },
    logger_{ logger },
    keylist_{ keylist },
    bkey_{ 100, ' ' },
    ckey_{ 100, ' ' }
{
}

BlockHandler::~BlockHandler( void )
{
}

long BlockHandler::setRecordStartOffset( FILE* inf, long soff )
{
    const static std::string fnname{"setRecordStartOffset"};
    long i;
    char c;

    // soff bounds checking.
    if ( soff < 0 ) soff = 0;
    if ( soff >= ifsize_ ) soff = ifsize_ - 1;  // the last character in the file.

    // process from soff backward to the begin.
    i = soff;
    // backup until we hit the header or byte 0.
    while ( i > header_.length() ) {
        if ( fseek( inf, i, SEEK_SET ) != 0 ) {
            // this will trigger if the file is empty.
            logger_->error( "{} fseek to {} returned non-zero.", fnname, i );
            return -1;
        }

        // get the character at i.
        if ( ( c = fgetc( inf ) ) != EOF ) {
            // if a record terminator AND it wasn't the FIRST character, tell the next position: soff of record we were
            // on.
            if ( c == FileSplitter::rdelim && i < soff ) {
                return ftell( inf );
            }

        } else {
            logger_->warn( "{} EOF while searching backward in a file (shouldn't happen).", fnname );
            return -1;
        }
        i--;
    }

    if ( fseek( inf, i, SEEK_SET ) != 0 ) {
        logger_->error( "{} fseek to {} returned non-zero.", fnname, i );
        return -1;
    }
    return i;
}

long BlockHandler::setRecordMultiKey( FILE* f, long soff, std::string& key ) 
{
    const static std::string fnname{"setRecordMultiKey"};
    char c;
    long rsoff;
    uint32_t fdc{ 1 };           // field delimiter count.
    bool collect{ true };

    rsoff = setRecordStartOffset( f, soff );

    if ( rsoff < 0 ) {
        logger_->error( "{} problem setting the record soff offset.", fnname );
        return -1;
    }

    // reset the "write" position into this string to the first address (the old key can be thrown away).
    key.clear();                                    
    // iterate through the key indicies we want to collect.
    //std::vector<uint32_t>::const_iterator kit = keylist_.begin();
    auto kit = keylist_.begin();

    // build the key for this record from the start of this line in the file.
    // we keep working while the key index list still has keys (it should be in order) AND
    // the field delimiter count (how we find the right columns) remains less than the key index AND
    // have a character from the line and we haven't reached the EOF.
    // while ( kit != keylist_.end() && fdc <= *kit && (( c = fgetc(f) ) != EOF )) {
    while ( kit != keylist_.end() && (( c = fgetc(f) ) != EOF )) {
        if ( c == FileSplitter::rdelim ) break;
        if ( c == FileSplitter::fdelim ) {
            fdc++;
            if (fdc > *kit) {
                ++kit;
                if ( kit != keylist_.end() ) key.push_back('.');
            }
        } else {
            if ( fdc == *kit ) key.push_back(c);
        }
    }

    if ( fseek( f, rsoff, SEEK_SET ) != 0 ) {
        logger_->error( "{} fseek to {} returned non-zero.", fnname, rsoff );
        return -1;
    }

    logger_->trace( "{} key = {}; set position to start = {}", fnname, key, rsoff );
    return ftell(f);
}

long BlockHandler::setRecordKey( FILE* f, long soff, std::string& key ) 
{
    const static std::string fnname{"setRecordKey"};
    char c;
    long rsoff;

    rsoff = setRecordStartOffset( f, soff );

    if ( rsoff < 0 ) {
        logger_->error( "{} problem setting the record soff offset.", fnname );
        return -1;
    }

    // reset the "write" position into this string to the first address (the old key can be thrown away).
    key.clear();                                    
    while ( ( c = fgetc(f) ) != EOF ) {
        if ( c == FileSplitter::fdelim || c == FileSplitter::rdelim ) {
            break;                                                     // finished collecting the key.
        }
        key.push_back(c);
    }

    if ( fseek( f, rsoff, SEEK_SET ) != 0 ) {
        logger_->error( "{} fseek to {} returned non-zero.", fnname, rsoff );
        return -1;
    }

    logger_->trace( "{} key = {}; set position to start = {}", fnname, key, rsoff );
    return ftell(f);
}

long BlockHandler::findFirstRecord( FILE* f, long soff, long end )
{
    const static std::string fnname{"findFirstRecord"};
    long cpos;
    long begin = header_.length();

    // boundary checking: soff \in [0,ifsize_]
    if ( end > ifsize_ ) end = ifsize_;
    if ( soff > end ) soff = end;
    if ( soff < begin ) soff = begin;

    // Get the record key for the record containing the starting offset.
    //if ( (end = setRecordKey( f, soff, bkey_ )) < 0 ) {
    if ( (end = setRecordMultiKey( f, soff, bkey_ )) < 0 ) {
        logger_->error( "{} error code from setRecordKey.", fnname );
        return -1;
    }

    soff = std::ceil( (begin + end) / 2.0);

    // as intended, this loop will not be entered if we are searching anywhere in the first record.
    while ( soff > begin && soff < end ) {

        //cpos = setRecordKey( f, soff, ckey_ );
        cpos = setRecordMultiKey( f, soff, ckey_ );

        if ( ckey_ == bkey_ ) {
            // continue to jump toward beginning of file.
            end = cpos;

        } else {
            // jump toward end of file.
            begin = soff;
        }

        soff = std::ceil( (begin + end) / 2.0);
        logger_->trace( "{} ckey={} bkey={} begin={} cpos={} end={} soff={}", fnname, ckey_, bkey_, begin, cpos, end, soff );
    }

    // make sure we are at the beginning of the record
    return setRecordStartOffset( f, soff );
}

void BlockHandler::operator()( long begin, long end )
{
    const static std::string fnname{"BH Runner"};

    std::string fn{ odname_ };

    FILE* inf = fopen( ifname_.c_str(), "rb" );

    logger_->trace( "{} block original bounds [{},{})", fnname, begin, end );
    if ( (begin = findFirstRecord( inf, begin, end )) < 0 ) {
        return;
    }

    logger_->trace( "{} block new start: {} with key: {}.", fnname, begin, bkey_ );

    fn += bkey_;

    if ( end >= ifsize_ ) end = ifsize_;
    else {
        // search for the first record starting on the last line.
        if ( (end = findFirstRecord( inf, end, end )) < 0 ) {
            return;
        }
    }

    logger_->trace( "{}: block new end: {} with key: {}.", fnname, end, bkey_ );

    fn += "-" + bkey_ + ".csv";

    if ( begin == end ) {
        logger_->trace( "{}: homogeonous key, so empty block.", fnname );
    }

    long total_bytes = end - begin;
    logger_->trace( "{}: Total Bytes to Write: {}.", fnname, total_bytes );

    // for testing, just write the entire block at key boundaries for this block.
    // transfer( begin, end - begin, fn ); 

    // move from back to front now that we have our boundaries and write out each block.
    // the search is a binary search (logarithmic time).
    std::string ofname{};
    while ( end > begin ) {
        // end - 1 moves into the last byte of the block of interest or begin = end - 1 which will halt execution.
        long epos = findFirstRecord( inf, end - 1, end );
        ofname = odname_ + bkey_ + ".csv";
        long r = transfer( epos, end - epos, ofname );
        logger_->trace( "{}: begin: {} epos: {} end: {}", fnname, begin, epos, end);
        logger_->trace( "{}: Attempting to write: {}; Wrote {} bytes for key {}", fnname, end-epos, r, bkey_ );
        total_bytes -= r;
        end = epos;
    }
    logger_->trace( "{}: Output Bytes Status: {}.", fnname, total_bytes );
}

long BlockHandler::transfer( long soff, long bytes_to_write, const std::string& ofn )
{
    const static std::string fnname{"transfer"};
    long bytes_read{0};
    long bytes_written{0};
    long block_size{0};
    long total_bytes{0};

    FILE* source = fopen( ifname_.c_str(), "rb" );
    FILE* dest = fopen( ofn.c_str(), "wb" );

    if (!source) {
        logger_->error( "{} Failed to open source file: {}", fnname, ifname_ );
        return -1;
    }

    if (!dest) {
        logger_->error( "{} Failed to open destination file: {}", fnname, ofn );
        return -1;
    }

    if ( header_.length() > 0 ) {
        // header includes the newline.
        fwrite( header_.c_str(), sizeof(char), header_.length(), dest );
    }

    if ( fseek( source, soff, SEEK_SET ) != 0 ) {
        logger_->error( "{} failure to seek to soff of source block: {}", fnname, soff );
        return total_bytes;
    }

    while ( bytes_to_write > 0 && !feof( source ) ) {

        block_size = ( bytes_to_write < BUFSIZE ) ? bytes_to_write : BUFSIZE;

        bytes_read    = fread( buf, sizeof buf[0], block_size, source );
        bytes_written = fwrite( buf, sizeof buf[0], bytes_read, dest );
        total_bytes += bytes_written;

        if ( bytes_written != bytes_read ) {
            logger_->trace("{} write size: {} does not match the read size: {}.", fnname, bytes_written, bytes_read );
            break;
        }

        bytes_to_write -= bytes_read;
    }

    fclose( source );
    fclose( dest );
    return total_bytes;
}

int main( int argc, char* argv[] )
{
    FileSplitter fs{"filesplitter","  Split single large CSV files into individual files having unique keys.\n  Individual files are named based on their unique keys.\n  Keys can be made up of multiple fields/columns in the CSV file.\n  Splitting is made more efficient in two ways:\n    1. Multiple threads can be used.\n    2. Binary search is done to find the break points.\n    3. All operations on at the byte-level, not the line level.\n  CAUTION: The large file must be sorted by the key used to split."};
    fs.addOption( 'h', "help", "print out some help" );
    fs.addOption( 'H', "header", "The first line in the file is a header line." );
    fs.addOption( 't', "threads", "The number of threads to use to process the file.", true );
    fs.addOption( 'v', "verbose", "The log level [trace,debug,info,warning,error,critical,off]", true );
    fs.addOption( 'o', "outdir", "The directory in which to put the output", true, "output" );
    fs.addOption( 'L', "logdir", "The directory in which to put the logs", true );
    fs.addOption( 'k', "key", "The data field indices (1-based column numbers) used to define the key to split the files", true );

    try {

        if (!fs.parseArgs(argc, argv)) {
            // cli arguments could not be parsed.
            fs.usage();
            std::exit( EXIT_FAILURE );
        }

        if (fs.getOption('h').isSet()) {
            fs.help();
            std::exit( EXIT_SUCCESS );
        }

    } catch (std::exception& e) {

        std::cerr << e.what() << "\n";
        std::exit( EXIT_FAILURE );

    }

    // run the file splitter and exit with the tool's return code.
    std::exit( fs.splitFile() );
}

