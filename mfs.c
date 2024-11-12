#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#define WHITESPACE " \t\n"      /* We want to split our command line up into tokens
                                   so we need to define what delimits our tokens.
                                   In this case  white space
                                   will separate the tokens on our command line */

#define MAX_COMMAND_SIZE 255    // The maximum command-line size

#define MAX_NUM_ARGUMENTS 11    

struct __attribute__((__packed__)) DirectoryEntry {
  char      DIR_Name[11];
  uint8_t   DIR_Attr;
  uint8_t   Unused1[8];
  uint16_t  DIR_FirstClusterHigh;
  uint8_t   Unused2[4];
  uint16_t  DIR_FirstClusterLow;
  uint32_t  DIR_FileSize;
};

struct Fat32Info {
  // fat 32 info data'
  int16_t BPB_BytesPerSec;
  int8_t BPB_SecPerClus;
  int16_t BPB_RsvdSecCnt;
  int8_t BPB_NumFATS;
  int32_t BPB_FATSz32;
  int16_t BPB_ExtFlags;
  int32_t BPB_RootClus;
  int16_t BPB_FSInfo;
  // calculated addresses
  int32_t FAT1Address;
  int32_t TotalFatSize;
  int32_t RootDir;
  int16_t ClusterSize;
};

void cdCommand();
void lsCommand();
void openCommand();
void openFAT();
void closeFAT();
void info();
void stat(char *name);
void del(char *name);
void undel(char *name);
void readFile(char *name, uint32_t position, int numBytes);
void get(char *name, char *new_name);
void put(char *name, char *new_name);
int caseInsenstiveMatching(char *userFile, char *linuxFile);
void findLinuxDirFile(char *name, char *linuxFile);
uint32_t findEmptyCluster();
void markClusterToBeUsed(uint32_t clusterNum, uint32_t nextCluster);
// void readRootDir();
void readHardCodedDir();
void readDir();
void listDir();
// void getParentDir();
void getSubdir(char *name);
uint32_t LBAToOffset(int32_t sector);
int16_t NextLB( uint32_t sector);
int compareUserString(char *userName, char *dirName);
void convertNameToUpper(char *userName);
// void convertNewNameToExpanded(char *name);

// GLOBALS
// initialize to null
FILE *fp = NULL;
int32_t current_working_dir = 0;
int offset = 0;

// Fat32 info 
struct Fat32Info f32_info;
// Directory struct
struct DirectoryEntry dir[16];
/* Parse input */
char *token[MAX_NUM_ARGUMENTS];
int flag_exit_while_global = 1;
char expanded_user_name[12];
// char new_expanded_name[12];

int main( int argc, char * argv[] )
{
  // grab our cmd string in put it into heap
  char * command_string = (char*) malloc( MAX_COMMAND_SIZE );

  while( flag_exit_while_global )
  {   
    
    // interactive mode
    // Read the command from the command line.  The
    // maximum command that will be read is MAX_COMMAND_SIZE
    // This while command will wait here until the user inputs something.
    // Print out the msh prompt and take user input
    printf ("mfs> ");
    while( !fgets (command_string, MAX_COMMAND_SIZE, stdin) );

    int token_count = 0;
                                                        
    // Pointer to point to the token
    // parsed by strsep
    char *argument_pointer;                         
    char *working_string  = strdup( command_string );

    // Make all of our input UPPERCASE
    // will make it case insensitive
    for(int i = 0; i < 255 && working_string[i] != '\0'; i++)
    {
      working_string[i] = toupper(working_string[i]);
    }

    // we are going to move the working_string pointer so
    // keep track of its original value so we can deallocate
    // the correct amount at the end
    char *head_ptr = working_string;
    
    // Tokenize the input with whitespace used as the delimiter
    int found_command = 0; 
    while ( ( (argument_pointer = strsep(&working_string, WHITESPACE ) ) != NULL) && (token_count<MAX_NUM_ARGUMENTS))
    {
      // skip leading whitespace tokens
      if (!found_command && strlen(argument_pointer) == 0)
      {
        continue;
      }

      // copy the argument to our specific token
      token[token_count] = strndup( argument_pointer, MAX_COMMAND_SIZE );

      // if we run into a empty token, make it null
      if( strlen( token[token_count] ) == 0 )
      {
        token[token_count] = NULL;
      }
      else if (!found_command)
      {
        // turn on our flag to stop trimming whitespacesa
        found_command = 1;
      }
      token_count++;
        
    }

    // Shell functionalilty goes here!

    int token_index  = 0;
    for( token_index = 0; token_index < token_count; token_index ++ ) 
    {
      // DEBUG print for token checking
      // printf("token[%d] = %s\n", token_index, token[token_index] );  

      // if the user hits enter, we want to print a new msh line
      if(token[0] == NULL)
      {
        continue;
      }

      /********** Internal Functions **********/

      // want to run our commands only once
      if(token_index == 0)
      {
        // EXIT
        // Exit iff there are no args after exit
        if ( ((strcmp("EXIT", token[0])) == 0)) 
        {
          // if arguments passed throw an error (shouldn't have args on exit)
          if (token[1] != NULL)
          {
            // Error to pass args to quit || exit
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));  
            // continue to loop again
            continue; 
          } 
          else 
          {
            flag_exit_while_global = 0;
          }
        }
        // QUIT
        // quit iff there are no args after quit
        else if ( ((strcmp("QUIT", token[0])) == 0))
        {
          // if arguments passed throw an error (shouldn't have args on quit)
          if (token[1] != NULL)
          {
            // Error to pass args to quit || exit
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message)); 
            continue;  
          } 
          else 
          {
            flag_exit_while_global = 0;
          }
        }

        // OPEN
        // open <filename>
        // This command shall open a fat32 image. Filenames of fat32 images shall not contain spaces and shall be limited to 100 characters.
        else if ( ((strcmp("OPEN", token[0])) == 0 ))
        {
          openCommand();
        }
          
        // CLOSE
        // close
        // This command shall close the fat32 image. 
        else if ( ((strcmp("CLOSE", token[0])) == 0 ))
        {
          // close our FAT
          closeFAT();
        }

        // INFO
        // info
        // This command shall print out information about the file system in both hex and decimal
        else if ( ((strcmp("INFO", token[0])) == 0 ))
        {
          // printf info of FAT
          info();
        }

        // STAT
        // stat <filename> or <directory name>
        // This command prints the attributes and starting cluster number of the file or dir name
        // if param is dir name, then the size shall be 0.
        // if the file or dir doesnt exist, print "Error: file not found"
        else if ( ((strcmp("STAT", token[0])) == 0) )
        {
          stat(token[1]);
        }

        // DEL
        // del <filename>
        // Marks file as deleted (data still persists on image, but can be overwritten)
        else if( ((strcmp("DEL", token[0])) == 0 ) )
        {
          del(token[1]);
        }

        // UNDEL
        // undel <filename>
        // un-deletes the file from the file sys
        else if ( ((strcmp("UNDEL", token[0])) == 0 ) )
        {
          undel(token[1]);
        }

        // READ
        // read <filename> <position> <number of bytes> <OPTION>
        else if ( ((strcmp("READ", token[0])) == 0 ) )
        {
          if (token[1] == NULL || token[2] == NULL || token[3] == NULL)
          {
            printf("Error not enough args. \n expected: read <filename> <position> <number of bytes> <OPTION>\n");
            break;
          }
          uint32_t position = atoi(token[2]);
          uint32_t numBytes = atoi(token[3]);

          readFile(token[1], position, numBytes);
        }

        // GET
        // get <filename> || <new filename>
        // This command shall retrieve the file from the FAT 32 image and place it in your current working directory. 
        // new renames it w/ new name
        // If the file or directory does not exist then your program shall output “Error: File not found”.
        else if ( ((strcmp("GET", token[0])) == 0 ) )
        {
          get(token[1], token[2]);
        }
        // PUT
        // put <filename> || <new filename>
        // This command shall read a file from the current working directory and place it in the memory resident FAT 32 image.
        // w/ new name, it adds a new entry
        else if ( ((strcmp("PUT", token[0])) == 0 ) )
        {
          put(token[1], token[2]);
        }

        // LS
        // ls <.> || <..>
        // Lists the directory contents. Your program shall support listing “.” and “..” 
        else if ( ((strcmp("LS", token[0])) == 0 ))
        {
          lsCommand();
        }

        // CD
        // cd <directory> (supports . and ..)
        // This command shall change the current working directory to the given directory. 
        // Your program shall support relative paths, e.g. cd ../name and absolute paths.
        else if ( ((strcmp("CD", token[0])) == 0) )
        {
          cdCommand();
        }
      }
    }
    // clean up heap vars
    free( head_ptr );
  }
  
  if (fp != NULL)
  {
    closeFAT();
  }
  
  free(command_string);
  return 0;     
}

/************************ Helper Functions **************************/
 
void cdCommand()
{
  if (fp == NULL)
  {
    printf("Error: File system not open\n");
    return;
  } 

  if ( token[2] != NULL )
  {
    char error_message[30] = "An error more than 1 arg\n";
    write(STDERR_FILENO, error_message, strlen(error_message)); 
    return;
  }
  else if( token[1] == NULL )
  {
    // cd <NULL>

    // if we want cd to work like terminal this will take us to root
    // offset = f32_info.RootDir;
    printf("error not enough args\n");
    return;
  }
  else 
  {
    char *path;
    // determining absolute and relative path
    // relative path
    if (token[1][0] == '/')
    {
      offset = f32_info.RootDir;
      path = strdup(token[1] + 1); // Skip the leading '/'
    }
    // relative path
    else
    {
      path = strdup(token[1]);
      printf("path: %s\n", path);
    }

    char *refPtr;
    char *component = strtok_r(path, "/", &refPtr);

    while (component != NULL)
    {
      // before changing dirs, we need to read where we are and fill our dir struct
      readDir();
      if ( strncmp(".", component, 1) == 0 )
      {
        getSubdir(component);
      }
      else if ( strncmp("..", component, 2) == 0 )
      {
        // go up a dir
        getSubdir(component);
      }
      else
      {
        // get subdir
        getSubdir(component);
      }
      // keep on parsing
      component = strtok_r(NULL, "/ ", &refPtr);
    }

    // free our path var
    free(path);
    return;
  }
}

void lsCommand()
{
  if (fp == NULL)
  {
    // Check if we've opened the FAT
    printf("Error File system image must be opened first\n");
    return;
  }
  // by default we read where the user is
  if (token[1] == NULL)
  {
    // ls
    // list current directory
    listDir();
    return;
  }
  else if( strcmp(".", token[1]) == 0 )
  {
    // ls . 
    // list current directory
    listDir();
    return;
  }
  else if ( strcmp("..", token[1]) == 0 )
  {
    // ls ..
    // get parent dir then list it
    getSubdir(token[1]);
    listDir();
    return;
  }
}

void openCommand()
{
  // our 1st argument is expected to be the filename
  if ( token[1] != NULL )
  {
    // try opening fat
    openFAT();
    return;
  }
  else 
  {
    if( fp == NULL)
    {
      printf("Error: File system image not found\n");
      return;
    }
    else
    {
      printf("Error: File system image already open\n");
      printf("fp = %p\n", fp);
      return;
    }
  }
}

void openFAT()
{
  if (fp == NULL)
  {
    // we want to convert our filename to something that can be used
    char imgname[100];
    strcpy(imgname, token[1]);

    for (int i = 0; i < 100 && imgname[i] != '\0'; i++)
    {
      imgname[i] = tolower(imgname[i]);
    }

    fp = fopen(imgname, "r+");
    if (fp == NULL)
    {
      printf("Error: File system image not found\n");
      return;
    }

    // read in some basic info for our fat file structure
    // fread() reads nmemb items of data, each size bytes long, from the stream pointed to by stream, storing them at the location given by ptr.
    // Read BytesPerSec
    fseek(fp, 11, SEEK_SET);
    fread(&f32_info.BPB_BytesPerSec, 2, 1, fp);
    // read SecPerClus
    fseek(fp, 13, SEEK_SET);
    fread(&f32_info.BPB_SecPerClus, 1, 1, fp);
    // read RsvdSecCnt
    fseek(fp, 14, SEEK_SET);
    fread(&f32_info.BPB_RsvdSecCnt, 2, 1, fp);
    // read NumFATS
    fseek(fp, 16, SEEK_SET);
    fread(&f32_info.BPB_NumFATS, 1, 1, fp);
    // read FATSz32
    fseek(fp, 36, SEEK_SET);
    fread(&f32_info.BPB_FATSz32, 4, 1, fp);
    // read ExtFlags
    fseek(fp, 40, SEEK_SET);
    fread(&f32_info.BPB_ExtFlags, 2, 1, fp);
    // read RootClus
    fseek(fp, 44, SEEK_SET);
    fread(&f32_info.BPB_RootClus, 4, 1, fp);
    // read FSInfo
    fseek(fp, 48, SEEK_SET);
    fread(&f32_info.BPB_FSInfo, 2, 1, fp);
    // calculations using info
    f32_info.FAT1Address = f32_info.BPB_RsvdSecCnt * f32_info.BPB_BytesPerSec;
    f32_info.TotalFatSize = f32_info.BPB_NumFATS * f32_info.BPB_FATSz32 * f32_info.BPB_BytesPerSec;
    f32_info.RootDir = f32_info.TotalFatSize + f32_info.FAT1Address;
    // offset is root by default
    offset = f32_info.RootDir;
    // current working dir of root is 0 or cluster number of it
    current_working_dir = 0;
    f32_info.ClusterSize = f32_info.BPB_SecPerClus * f32_info.BPB_BytesPerSec;

    // fseek(fp, 17, SEEK_SET);
    // int16_t rootEnt;
    // fread(&rootEnt, 2, 1, fp);
    // printf("rootEnt: 0x%x | %d\n", rootEnt, rootEnt);

    // reset fp
    fseek(fp, 0, SEEK_SET);

    // we must also fill out our dir file structure too
    readDir();

    // fseek(fp, 0x4000, SEEK_SET);
    // for (int i = 0; i < 10; i++)
    // {
    //   int32_t a32int; 
    //   fread(&a32int, 2, 1, fp);
    //   printf("reading a32int: 0x%x | %d | i = %d\n", a32int, a32int, i);
    // }

    // printf("file opened: fp == %p\n", fp);
    return;

  }
  else 
  {
    printf("Error: File system image already open\n");
    printf("fp = %p\n", fp);
    return;
  }
}

void closeFAT()
{
  // file is not open
  if (fp == NULL)
  {
    printf("Error: File system not open\n");
    return;
  }
  else
  {
    // file now closes
    printf("closing FAT32...\n");
    // fclose(fp);
    free(fp);
    fp = NULL;
    printf("fp = %p\n", fp);
    return;
  }
}

void info()
{
  if (fp == NULL)
  {
    printf("Error File system image must be opened first\n");
    return;
  }

  printf("INFO           \tDecimal\tHex\n");
  printf("BPB_BytesPerSec\t%d\t0x%x\n", f32_info.BPB_BytesPerSec, f32_info.BPB_BytesPerSec);
  printf("BPB_SecPerClus \t%d\t0x%x\n", f32_info.BPB_SecPerClus, f32_info.BPB_SecPerClus);
  printf("BPB_RsvdSecCnt \t%d\t0x%x\n", f32_info.BPB_RsvdSecCnt, f32_info.BPB_RsvdSecCnt);
  printf("BPB_NumFATS    \t%d\t0x%x\n", f32_info.BPB_NumFATS, f32_info.BPB_NumFATS);
  printf("BPB_FATSz32    \t%d\t0x%x\n", f32_info.BPB_FATSz32, f32_info.BPB_FATSz32);
  printf("BPB_ExtFlags   \t%d\t0x%x\n", f32_info.BPB_ExtFlags, f32_info.BPB_ExtFlags);
  printf("BPB_RootClus   \t%d\t0x%x\n", f32_info.BPB_RootClus, f32_info.BPB_RootClus);
  printf("BPB_FSInfo     \t%d\t0x%x\n", f32_info.BPB_FSInfo, f32_info.BPB_FSInfo);

  // printf("\nFAT1Address    \t%d\t0x%x\n", f32_info.FAT1Address, f32_info.FAT1Address);
  // printf("TotalFatSize   \t%d\t0x%x\n", f32_info.TotalFatSize, f32_info.TotalFatSize);
  // printf("ClusterStartAdd\t%d\t0x%x\n", f32_info.RootDir, f32_info.RootDir);
  // printf("currentwrkngDir\t%d\t0x%x\n", current_working_dir, current_working_dir);
  // printf("ClusterSize    \t%d\t0x%x\n", f32_info.ClusterSize, f32_info.ClusterSize);

  return;
}

void stat(char *name)
{
  // ERR checking
  if (fp == NULL)
  {
    printf("Error File system image must be opened first\n");
    return;
  }
  else if (token[1] == NULL)
  {
    printf("Not enough Parameters.\n Expected: <filename> OR <dir name>\n\n");
    return;
  }
  // cover . and .. edge cases
  else if( (strncmp(name, "..", 2) == 0) ||  (strncmp(name, ".", 1) == 0))
  {
    printf("not supported\n");
    return;
  }

  convertNameToUpper(name);

  int nextBlk = 0;
  // read in current directory
  do
  {
    readDir();
    // loop through our dir struct
    for (int i = 0; i < 16; i++)
    {
      // find the matching dir name and make sure its size is 0
      if ( (compareUserString(expanded_user_name, dir[i].DIR_Name)) && dir[i].DIR_Attr == 0x10)
      {
        int16_t clusterHigh = dir[i].DIR_FirstClusterHigh;
        uint16_t clusterLow = dir[i].DIR_FirstClusterLow;
        uint32_t combinedCluster = ( (uint32_t)clusterHigh << 16) + ((uint32_t)clusterLow);


        printf("DIR: %s | Attributes: 0x%x | Starting Cluster: 0x%x | size: 0x0\n", expanded_user_name, dir[i].DIR_Attr, combinedCluster);
        return;
      }
      else if (compareUserString(expanded_user_name, dir[i].DIR_Name) && (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x20) )
      {
        int16_t clusterHigh = dir[i].DIR_FirstClusterHigh;
        uint16_t clusterLow = dir[i].DIR_FirstClusterLow;
        uint32_t combinedCluster = ( (uint32_t)clusterHigh << 16) + ((uint32_t)clusterLow);

        printf("FILE: %s | Attributes: 0x%x | Starting Cluster: 0x%x | size: 0x%x\n", expanded_user_name, dir[i].DIR_Attr, combinedCluster, dir[i].DIR_FileSize);
        return;
      }
    }
    nextBlk = NextLB(current_working_dir);
    if ( nextBlk >= 0 ) 
    {
      offset = LBAToOffset(nextBlk);
    }

  }
  while (nextBlk >= 0);


  printf("Error: File not found.\n");

}

void del(char *name)
{
  // ERR checking
  if (fp == NULL)
  {
    printf("Error File system image must be opened first\n");
    return;
  }
  else if (token[1] == NULL)
  {
    printf("Not enough Parameters.\n Expected: <filename>\n\n");
    return;
  }
  // cover . and .. edge cases
  else if( (strncmp(token[1], "..", 2) == 0) ||  (strncmp(token[1], ".", 1) == 0))
  {
    printf("ERR: cannot delete . and ..\n");
    return;
  }

  // we have to make sure we convert our user input to something readable by our filesys
  convertNameToUpper(name);

  int nextBlk = 0;
  // read in current directory
  do
  {
    readDir();
    for (int i = 0; i < 16; i++)
    {
      // check if file has already been deleted
      // we need to modify expanded_name's entry a bit
      char savedChar = expanded_user_name[0];
      expanded_user_name[0] = (char)0xe5;
      if ((strncmp(dir[i].DIR_Name, expanded_user_name, 11) == 0 ) && dir[i].DIR_Name[0] == (char)0xe5)
      {
        printf("Error: file already deleted\n");
        return;
      }
      else
      {
        // restore expanded name
        expanded_user_name[0] = savedChar;
      }

      // find the file
      if ( (strncmp(dir[i].DIR_Name, expanded_user_name, 11) == 0 ) && (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x20 ) )
      {

        // first in our dir struct we mark the file as deleted
        dir[i].DIR_Name[0] = (char)0xe5;

        // mark file as deleted
        // We need to calculate the offset of our file using our current dir
        // since our dir struct is packed we can do this
        uint32_t fileOffset = offset + (i * (sizeof(struct DirectoryEntry)));

        // now we go to file in our fat32.img and update the first char value (which is a byte)
        fseek(fp, fileOffset, SEEK_SET);
        fwrite(&dir[i], sizeof(struct DirectoryEntry), 1, fp);
        fflush(fp);

        rewind(fp);

        
        printf("deleted file %s\n", expanded_user_name);
        return;
      }

      // if the user tries to pass in a directory, throw error
      if((strncmp(dir[i].DIR_Name, expanded_user_name, 11) == 0 ) && dir[i].DIR_Attr == 0x10)
      {
        printf("Error: cannot delete directories\n");
        return;
      }
    }
    nextBlk = NextLB(current_working_dir);
    if ( nextBlk >= 0 ) 
    {
      offset = LBAToOffset(nextBlk);
    }

  }
  while (nextBlk >= 0);

  printf("Error: file not found\n");
  return;

}

void undel(char *name)
{
  // ERR checking
  if (fp == NULL)
  {
    printf("Error File system image must be opened first\n");
    return;
  }
  else if (token[1] == NULL)
  {
    printf("Not enough Parameters.\n Expected: <filename>\n\n");
    return;
  }
  // cover . and .. edge cases
  else if( (strncmp(token[1], "..", 2) == 0) ||  (strncmp(token[1], ".", 1) == 0))
  {
    printf("Error: cannot undel '.' and '..' they already are undeleted\n");
    return;
  }

  // we have to make sure we convert our user input to something readable by our filesys
  convertNameToUpper(name);

  int nextBlk = 0;
  // read in current directory
  do
  {
    readDir();
    for (int i = 0; i < 16; i++)
    {
      // goal is to check the file dir for an undeleted file, note the only different char is the del char

      // we need to look for a deleted file
      // we need to modify expanded_name's entry a bit
      char savedChar = expanded_user_name[0];
      expanded_user_name[0] = (char)0xe5;
      if ((strncmp(dir[i].DIR_Name, expanded_user_name, 11) == 0 ) && dir[i].DIR_Name[0] == (char)0xe5 )
      {
        // restore expanded name
        expanded_user_name[0] = savedChar;
        // first in our dir struct we mark the file as undeleted w/ the char the user gave us
        dir[i].DIR_Name[0] = expanded_user_name[0];

        // we need the file offset
        uint32_t fileOffset = offset + (i * (sizeof(struct DirectoryEntry)));

        // now we go to file in our fat32.img and update the first char value (which is a byte)
        fseek(fp, fileOffset, SEEK_SET);
        fwrite(&dir[i], sizeof(struct DirectoryEntry), 1, fp);
        fflush(fp);

        rewind(fp);

        printf("un-deleted file %s\n", expanded_user_name);
        return;
      }
      // restore expanded name
      expanded_user_name[0] = savedChar;

      // find the file
      if ( (strncmp(dir[i].DIR_Name, expanded_user_name, 11) == 0 ) && (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x20 ) )
      {
        printf("Error: cannot un-delete a non-deleted file\n");
        return;
      }

      // if the user tries to pass in a directory, throw error
      if((strncmp(dir[i].DIR_Name, expanded_user_name, 11) == 0 ) && dir[i].DIR_Attr == 0x10)
      {
        printf("Error: cannot un-delete directories\n");
        return;
      }
    }
    
    nextBlk = NextLB(current_working_dir);
    if ( nextBlk >= 0 ) 
    {
      offset = LBAToOffset(nextBlk);
    }

  }
  while (nextBlk >= 0);


  printf("Error: file not found\n");
  return;
}

void readFile(char *name, uint32_t position, int numBytes)
{
  // ERR checking
  if (fp == NULL)
  {
    printf("Error File system image must be opened first\n");
    return;
  }
  // must have args to use the function
  else if (name == NULL)
  {
    printf("Error not enough args. \n expected: read <filename> <position> <number of bytes> <OPTION>\n");
    return;
  }

  /*Reads from the given file at the position, in bytes, 
  specified by the position parameter and output the number of bytes specified. 
  The values shall be printed as hexadecimal integers in the form 0xXX by default.*/

  // converts input name to global name expanded_name
  convertNameToUpper(name);
  
  // update our dir struct before hand
  readDir();

  // find our file

  int nextBlk = 0;
  // read in current directory
  do
  {
    readDir();

    for (int i = 0; i < 16; i++)
    {
      // if we find the file
      if ( (strncmp(expanded_user_name, dir[i].DIR_Name, 11)) == 0 )
      {
        // get to the location of our file
        int16_t clusterHigh = dir[i].DIR_FirstClusterHigh;
        uint16_t clusterLow = dir[i].DIR_FirstClusterLow;
        uint32_t combinedCluster = ( (uint32_t)clusterHigh << 16) + ((uint32_t)clusterLow);
          
        // allocate the size of our buffer dynamically
        char *buffer = (char *)malloc(numBytes + 1);
        if (buffer == NULL)
        {
          printf("Error: Memory Allocation failed\n");
          free(buffer);
          return;
        }

        // null-terminate the buffer for saftety
        buffer[numBytes] = '\0'; 

        // we loop through various clusters
        while ( numBytes > f32_info.BPB_BytesPerSec && combinedCluster > 0)
        {
          // fseek to our file location
          uint32_t offsetToFile = LBAToOffset(combinedCluster);
          fseek(fp, offsetToFile + position, SEEK_SET);

          // Check if the read position is beyond the file size
          if (position + numBytes > dir[i].DIR_FileSize)
          {
            printf("Error: Read exceeds file size\n");
            return;
          } 
          // make sure we only copy 512 bytes at a time per sector
          uint32_t bytesToRead = f32_info.BPB_BytesPerSec;
          
          // read in the file
          size_t result = fread(buffer, 1, bytesToRead, fp);
          if (result != bytesToRead)
          {
            printf("Error: could not read data from FAT32 image\n");
            return;
          }

          // check if param 4 was used
          if (token[4] == NULL)
          {
            // print in hex
            for (int j = 0; j < numBytes; j++)
            {
              printf("0x%x ", (unsigned char)buffer[j]);
            }
            printf("\n");
          }
          else if ( (strncmp(token[4], "-ASCII", 6) == 0 ) )
          {
            // print out in ascii
            printf("%s\n", buffer);
          }
          else if ( (strncmp(token[4], "-DEC", 4) == 0 ) )
          {
            // print in decimal
            for (int j = 0; j < numBytes; j++)
            {
              printf("%d ", (unsigned char)buffer[j]);
            }
            printf("\n");
          }
          else
          {
            printf("Option flag unknown\n");
            free(buffer);
            return;
          }

          numBytes -= f32_info.BPB_BytesPerSec;
          combinedCluster = NextLB(combinedCluster);
        }

        // copy the remaining bytes
        if (numBytes > 0)
        {
          uint32_t offsetToFile = LBAToOffset(combinedCluster);
          fseek(fp, offsetToFile, SEEK_SET);
          size_t result = fread(buffer, 1, numBytes, fp);
          if (result != numBytes)
          {
            printf("Error: could not read data from FAT32 image\n");
            return;
          }
        }

        // check if param 4 was used
        if (token[4] == NULL)
        {
          // print in hex
          for (int j = 0; j < numBytes; j++)
          {
            printf("0x%x ", (unsigned char)buffer[j]);
          }
          printf("\n");
          free(buffer);

          return;
        }
        else if ( (strncmp(token[4], "-ASCII", 6) == 0 ) )
        {
          // print out in ascii
          printf("%s\n", buffer);

          free(buffer);
          return;
        }
        else if ( (strncmp(token[4], "-DEC", 4) == 0 ) )
        {
          // print in decimal
          for (int j = 0; j < numBytes; j++)
          {
            printf("%d ", (unsigned char)buffer[j]);
          }
          printf("\n");
          free(buffer);
          return;
        }
        else
        {
          printf("Option flag unknown\n");
          free(buffer);
        }
      }
      
    }
    
    nextBlk = NextLB(current_working_dir);
    if ( nextBlk >= 0 ) 
    {
      offset = LBAToOffset(nextBlk);
    }

  }
  while (nextBlk >= 0);
  

  printf("Error: File not Found\n");
  return;

}

void get(char *name, char *newName)
{
  // ERR checking
  if (fp == NULL)
  {
    printf("Error File system image must be opened first\n");
    return;
  }
  // must have args to use the function
  else if (token[1] == NULL)
  {
    printf("Error not enough args. \n expected: get <filename> OR get <filename> <new filename>\n");
    return;
  }

  /*Reads from the given file at the position, in bytes, 
  specified by the position parameter and output the number of bytes specified. 
  The values shall be printed as hexadecimal integers in the form 0xXX by default.*/

  // we expand our input so we can use it
  char expandedFile[12];
  memset( expandedFile, ' ', 12); 

  char *tok = strtok(name, ".");
  strncpy( expandedFile, tok, strlen(tok));
  tok = strtok(NULL, ".");

  if (tok)
  {
    strncpy( (char*)(expandedFile+8), tok, strlen(tok));
  }
  expandedFile[11] = '\0';

  // if a second fild passed in we also expand it
  char expandedFile2[12];
  if (token[2] != NULL)
  {
    memset( expandedFile2, ' ', 12); 

    char *tok2 = strtok(name, ".");
    strncpy( expandedFile2, tok2, strlen(tok2));
    tok2 = strtok(NULL, ".");

    if (tok2)
    {
      strncpy( (char*)(expandedFile2+8), tok2, strlen(tok2));
    }
    expandedFile2[11] = '\0';
  }

  int nextBlk;
  // loop through our dir struct
  do
  {
    readDir();

    // find our file
    for (int i = 0; i < 16; i++)
    {
      // if we find the file we want to put on the users filesys
      if ( (strncmp(expandedFile, dir[i].DIR_Name, 11)) == 0 )
      {
        // get to the location of our file
        int16_t clusterHigh = dir[i].DIR_FirstClusterHigh;
        uint16_t clusterLow = dir[i].DIR_FirstClusterLow;
        uint32_t combinedCluster = ( (uint32_t)clusterHigh << 16) + ((uint32_t)clusterLow);

        // open a new File pointer to write to that file
        FILE *getFile = NULL;
        // use either new name or current filename
        if (token[2] != NULL) 
        {
          getFile = fopen(newName, "w+");
        }
        else
        {
          getFile = fopen(name, "w+");
        }

        // while loop to copy
        uint32_t bytesToCopy = dir[i].DIR_FileSize;
        uint8_t buffer[512];

        // we loop through various clusters
        while ( bytesToCopy > f32_info.BPB_BytesPerSec && combinedCluster > 0)
        {
          // fseek to our file location
          uint32_t offsetToFile = LBAToOffset(combinedCluster);
          fseek(fp, offsetToFile, SEEK_SET);
          // make sure we only copy 512 bytes at a time per sector
          uint32_t bytesToRead = f32_info.BPB_BytesPerSec;
          
          // read in the file
          size_t result = fread(buffer, 1, bytesToRead, fp);
          if (result != bytesToRead)
          {
            printf("Error: could not read data from FAT32 image\n");
            fclose(getFile);
            return;
          }

          size_t wrote = fwrite(buffer, 1, bytesToRead, getFile);
          if (wrote != bytesToRead)
          {
            printf("Error: could not write data to output file\n");
            fclose(getFile);
            return;
          }

          // traverse to next block of data in FAT
          bytesToCopy -= f32_info.BPB_BytesPerSec;
          combinedCluster = NextLB(combinedCluster);
        }

        // copy the remaining bytes
        if (bytesToCopy > 0)
        {
          uint32_t offsetToFile = LBAToOffset(combinedCluster);
          fseek(fp, offsetToFile, SEEK_SET);
          size_t result = fread(buffer, 1, bytesToCopy, fp);
          if (result != bytesToCopy)
          {
            printf("Error: could not read data from FAT32 image\n");
            fclose(getFile);
            return;
          }
          size_t wrote = fwrite(buffer, 1, bytesToCopy, getFile);
          if (wrote != bytesToCopy)
          {
            printf("Error: could not write data to output file\n");
            fclose(getFile);
            return;
          }
        }

        // print success message, and if newName isnt null (which means it was used) print that out
        printf("File %s writen as '%s'\n", name, (newName != NULL) ? newName : name);
        fclose(getFile);
        return;
      }
    }

    nextBlk = NextLB(current_working_dir);
    if ( nextBlk >= 0 ) 
    {
      offset = LBAToOffset(nextBlk);
    }

  }
  while (nextBlk >= 0);

  
  printf("Error file not found\n");
}

void put(char *name, char *newName)
{
  // put pseudo code
  /*
  int bytesToCopy = filesize

  while ( bytesToCopy > BPB_bytesPerSector)
  {
    offsetToFile = LBAToOffset(clusterNumber);
    fseek(fp, offsetToFile, SEEK_SET);
    fread() from file BPB_bytesPerSec
    find new cluster NextLb()
    Update teh FAT for current Cluster number
    BytesToCopy = BytesToCopy - BPB_bytesPerSec
  } 

  if (bytesToCopy > 0)
  {
    offsetToFile = LBAToOffset(clusterNumber);
    fseek(fp, offsetToFile, SEEK_SET);
    fread() from file BPB_bytesPerSec
  }

  */

  // ERR checking
  if (fp == NULL)
  {
    printf("Error File system image must be opened first\n");
    return;
  }
  // must have args to use the function
  else if (token[1] == NULL)
  {
    printf("Error not enough args. \n expected: get <filename> OR get <filename> <new filename>\n");
    return;
  }

  // obtain the linux file name
  char linuxFile[512];
  findLinuxDirFile(name, linuxFile);
  if (linuxFile == NULL)
  {
    printf("Error file not found\n");
    return;
  }

  printf("linuxFile %s\n", linuxFile);

  // open the file 
  FILE *lFp = fopen(linuxFile, "r");
  if (lFp == NULL)
  {
    perror("Error: opening file");
    return;
  }

  // get the linux file size in bytes
  fseek(lFp, 0, SEEK_END);
  uint32_t lFileSz = ftell(lFp);
  rewind(lFp);



  /* 
  !we need to do a few things
  
  * check the current dirs cluster to see if we can add an entry
    * if our current dir has deleted files we can overwrite them 

  !if our current dir is full
  we need to:
    * find a new empty cluster,
      * in order to find new empty cluster we must read clusters sequentially. Counting 512 bytes as 1 block
      * remember we start from cluster 0, which is root
    * mark it as in use and link the starting fat to the new fat
      * this means that we need the NextLB to point to that new cluster
  
  Once we find a dir entry for it, and a spot in memory of the fat
  we need to:
    * copy the bytes from the linux file, and write in blocks of 512
  */

  // first we need to read our current dir to see if we currently have space for an entry
  uint32_t bytesToCopy = lFileSz;
  printf("bytesToCopy for LinuxFile: %u\n", bytesToCopy); 

  // we expand our input so we can use it
  char expandedFile[12];
  memset( expandedFile, ' ', 12); 

  char *tok = strtok(name, ".");
  strncpy( expandedFile, tok, strlen(tok));
  tok = strtok(NULL, ".");

  if (tok)
  {
    strncpy( (char*)(expandedFile+8), tok, strlen(tok));
  }
  expandedFile[11] = '\0';

  // if a second fild passed in we also expand it
  char expandedFile2[12];
  if (token[2] != NULL)
  {
    memset( expandedFile2, ' ', 12); 

    char *tok2 = strtok(name, ".");
    strncpy( expandedFile2, tok2, strlen(tok2));
    tok2 = strtok(NULL, ".");

    if (tok2)
    {
      strncpy( (char*)(expandedFile2+8), tok2, strlen(tok2));
    }
    expandedFile2[11] = '\0';
  }

  int nextBlk;
  do
  {
    readDir();
    // loop through and find an deleted entry or an empty entry to put our file in
    for (int i = 0; i < 16; i++)
    {
      // find a deleted entry we can overwrite that with a new entry
      if ( dir[i].DIR_Name[0] == (char)0xe5 && i < 16)
      {
        // copy our expanded name into a deleted file
        if (token[2] != NULL)
        {
          strncpy(dir[i].DIR_Name, expandedFile2, 11);
        }
        else
        {
          strncpy(dir[i].DIR_Name, expandedFile, 11);
        }
        // mark file w/ archieve flag by default.
        dir[i].DIR_Attr = 0x20;
        // update how many bytes is stored
        dir[i].DIR_FileSize = lFileSz;
        
        // find a empty cluster to store it, in order to do this we need to search from root and find an empty cluster
        uint32_t newCluster = findEmptyCluster();
        printf("new cluster = %d\n", newCluster);
        if (newCluster != 0)
        {
          // mark our new cluster to be used
          markClusterToBeUsed(newCluster, 0xFFFFFFF);
        }

        // shift our cnum by 16 to fit first cluster high
        // we want this to be stored in our struct to be written to the fat
        dir[i].DIR_FirstClusterHigh = newCluster >> 16;
        dir[i].DIR_FirstClusterLow= newCluster;

        printf("dirs fclusHigh: 0x%x | fclusLow: 0x%x\n", dir[i].DIR_FirstClusterHigh, dir[i].DIR_FirstClusterLow);

        // Writing to the current directory

        // we get the offset from the current directory and the amount of entries we have in our current struct
        uint32_t fileOffset = offset + (i * (sizeof(struct DirectoryEntry)));
        // now we go to file in our fat32.img and update the entry of our directory
        fseek(fp, fileOffset, SEEK_SET);
        fwrite(&dir[i], sizeof(struct DirectoryEntry), 1, fp);
        fflush(fp);
        rewind(fp);

        uint32_t newClusterOffset = LBAToOffset(newCluster);
        printf("new cluster offset: 0x%x\n", newClusterOffset);

        // now we can start writing to the offset 

        uint8_t buffer[512];
        uint32_t offsetToFile;

        // while our amt of bytes exceeds cluster sizes (512 bytes)
        while ( bytesToCopy > f32_info.BPB_BytesPerSec && newCluster > 0)
        {
          // make sure we only copy 512 bytes at a time per sector
          uint32_t bytesToRead = f32_info.BPB_BytesPerSec;
          
          // read in the file (dont have to worry abt moving lFp because reading does that)
          size_t result = fread(buffer, 1, bytesToRead, lFp);
          if (result != bytesToRead)
          {
            perror("Error: could not read data from FAT32 image");
            fclose(lFp);
            return;
          }

          // we get our offset to the file
          offsetToFile = LBAToOffset(newCluster);
          fseek(fp, offsetToFile, SEEK_SET);
          size_t wrote = fwrite(buffer, 1, bytesToRead, fp);
          if (wrote != bytesToRead)
          {
            perror("Error: could not write data");
            fclose(lFp);
            return;
          }

          // traverse to next block of data in FAT
          bytesToCopy -= f32_info.BPB_BytesPerSec;
          newCluster = NextLB(newCluster);
        } 

        // copy the remaining bytes
        if (bytesToCopy > 0)
        {
          // read from our linux file
          size_t result = fread(buffer, 1, bytesToCopy, lFp);
          // printf("result : %ld | bytesToCopy : %d\n", result, bytesToCopy);
          if (result != bytesToCopy)
          {
            perror("Error: could not read data (rb) | ");
            fclose(lFp);
            return;
          }

          // seek to our fat file and start writing to it
          offsetToFile = LBAToOffset(newCluster);
          // printf("offsetToFile: 0x%x\n", offsetToFile);
          fseek(fp, offsetToFile, SEEK_SET);
          size_t wrote = fwrite(buffer, 1, bytesToCopy, fp);
          if (wrote != bytesToCopy)
          {
            perror("Error: could not write data (rb)\n");
            fclose(lFp);
            return;
          }
        }

        printf("file %s successfully copied\n", linuxFile);
        fclose(lFp);
        return;
      }
    }

    nextBlk = NextLB(current_working_dir);
    if ( nextBlk >= 0 ) 
    {
      offset = LBAToOffset(nextBlk);
    }
  }
  while (nextBlk > 0); 

  // if theres no deleted entries we can fit our file in we need to append our current dir w/ a new one
  
  // search for an empty cluster
  // then mark the cluster as in use
  uint32_t newDirCluster = findEmptyCluster();

  if (newDirCluster != 0)
  {
    // link our CWD to the new clusters dir, and make sure our newDirCluster ends in the chain
    markClusterToBeUsed(current_working_dir, newDirCluster);
    markClusterToBeUsed(newDirCluster, 0xFFFFFFF);

    // get the offset of our new dir cluster
    uint32_t newClusterOffset = LBAToOffset(newDirCluster);
    // initialize the new dir cluster to 0 or empty entries
    uint8_t emptyEntry[sizeof(struct DirectoryEntry)] = {0};
    fseek(fp, newClusterOffset, SEEK_SET);
    fwrite(emptyEntry, (sizeof( struct DirectoryEntry) * 16), 1, fp);
    fflush(fp);

    // update the offset and current working cluster to use the new one
    offset = newClusterOffset;
    current_working_dir = newDirCluster;

    // now we can put our linux file in our fat
    readDir();
    // loop through and find an deleted entry or an empty entry to put our file in
    for (int i = 0; i < 16; i++)
    {
      // find an empty entry (0x00 means its never used)
      if ( dir[i].DIR_Name[0] == (char)0x00 && i < 16)
      {
        // copy our expanded name into a deleted file
        if (token[2] != NULL)
        {
          strncpy(dir[i].DIR_Name, expandedFile2, 11);
        }
        else
        {
          strncpy(dir[i].DIR_Name, expandedFile, 11);
        }
        // mark file w/ archieve flag by default.
        dir[i].DIR_Attr = 0x20;
        // update how many bytes is stored
        dir[i].DIR_FileSize = lFileSz;
        
        // find a empty cluster to store it, in order to do this we need to search from root and find an empty cluster
        uint32_t newCluster = findEmptyCluster();
        printf("new cluster = %d\n", newCluster);
        if (newCluster != 0)
        {
          // mark our new cluster to be used
          markClusterToBeUsed(newCluster, 0xFFFFFFF);
        }

        // shift our cnum by 16 to fit first cluster high
        // we want this to be stored in our struct to be written to the fat
        dir[i].DIR_FirstClusterHigh = newCluster >> 16;
        dir[i].DIR_FirstClusterLow= newCluster;

        printf("dirs fclusHigh: 0x%x | fclusLow: 0x%x\n", dir[i].DIR_FirstClusterHigh, dir[i].DIR_FirstClusterLow);

        // Writing to the current directory

        // we get the offset from the current directory and the amount of entries we have in our current struct
        uint32_t fileOffset = offset + (i * (sizeof(struct DirectoryEntry)));
        // now we go to file in our fat32.img and update the entry of our directory
        fseek(fp, fileOffset, SEEK_SET);
        fwrite(&dir[i], sizeof(struct DirectoryEntry), 1, fp);
        fflush(fp);
        rewind(fp);

        uint32_t newClusterOffset = LBAToOffset(newCluster);
        printf("new cluster offset: 0x%x\n", newClusterOffset);

        // now we can start writing to the offset 

        uint8_t buffer[512];
        uint32_t offsetToFile;

        // while our amt of bytes exceeds cluster sizes (512 bytes)
        while ( bytesToCopy > f32_info.BPB_BytesPerSec && newCluster > 0)
        {
          // make sure we only copy 512 bytes at a time per sector
          uint32_t bytesToRead = f32_info.BPB_BytesPerSec;
          
          // read in the file (dont have to worry abt moving lFp because reading does that)
          size_t result = fread(buffer, 1, bytesToRead, lFp);
          if (result != bytesToRead)
          {
            perror("Error: could not read data from FAT32 image");
            fclose(lFp);
            return;
          }

          // we get our offset to the file
          offsetToFile = LBAToOffset(newCluster);
          fseek(fp, offsetToFile, SEEK_SET);
          size_t wrote = fwrite(buffer, 1, bytesToRead, fp);
          if (wrote != bytesToRead)
          {
            perror("Error: could not write data");
            fclose(lFp);
            return;
          }

          // traverse to next block of data in FAT
          bytesToCopy -= f32_info.BPB_BytesPerSec;
          newCluster = NextLB(newCluster);
        } 

        // copy the remaining bytes
        if (bytesToCopy > 0)
        {
          // read from our linux file
          size_t result = fread(buffer, 1, bytesToCopy, lFp);
          // printf("result : %ld | bytesToCopy : %d\n", result, bytesToCopy);
          if (result != bytesToCopy)
          {
            perror("Error: could not read data (rb) | ");
            fclose(lFp);
            return;
          }

          // seek to our fat file and start writing to it
          offsetToFile = LBAToOffset(newCluster);
          // printf("offsetToFile: 0x%x\n", offsetToFile);
          fseek(fp, offsetToFile, SEEK_SET);
          size_t wrote = fwrite(buffer, 1, bytesToCopy, fp);
          if (wrote != bytesToCopy)
          {
            perror("Error: could not write data (rb)\n");
            fclose(lFp);
            return;
          }
        }

        printf("file %s successfully copied\n", linuxFile);
        fclose(lFp);
        return;
      }
    }

  }

  
  fclose(lFp);
  // printf("Error file not found\n");
}

// boolean checking
// checks if both files match
int caseInsenstiveMatching(char *userFile, char *linuxFile)
{
  // while we still have characters to compare
  while (*userFile && *linuxFile)
  {
    if (toupper(*userFile) != toupper(*linuxFile))
    {
      return 0;
    }
    // move ptrs forward and check until no chars left
    userFile++;
    linuxFile++;
  }
  return *userFile == *linuxFile;
}

void findLinuxDirFile(char *name, char *linuxFile)
{
  // initialize dirent ptr + structs
  DIR *dp;
  // to make this simple only open CWD
  dp = opendir(".");

  // if we actually open the CWD
  if (dp)
  {
    // intialize our linux dir struct
    struct dirent *lDir;
    // read our dir as long as it's not null
    while ( (lDir = readdir(dp)) != NULL )
    {
      // if we find a match
      if (caseInsenstiveMatching(name, lDir->d_name))
      {
        closedir(dp);
        // close and return our linux filename

        // make sure we append our dir name to have it be findable
        strcpy(linuxFile, "./");
        strcat(linuxFile, lDir->d_name);
        return;
      }
    }
  }
  // append a null when we cant find anything
  strcpy(linuxFile, "\0");
}

uint32_t findEmptyCluster()
{
  uint32_t emptyCluster = 0;
  // start at FAT1 address
  fseek(fp, f32_info.FAT1Address, SEEK_SET);

  // search for a empty cluster sequentially
  while (1)
  {
    uint32_t cluster;
    fread(&cluster, sizeof(uint32_t), 1, fp);

    if (cluster == 0x00000000)
    {
      // empty cluster found
      emptyCluster = ftell(fp) / sizeof(uint32_t) - f32_info.FAT1Address / sizeof(uint32_t);
      break;
    }
    
  }
  return emptyCluster;
}

void markClusterToBeUsed(uint32_t clusterNum, uint32_t nextCluster)
{
  // get the offset of the cluster
  uint32_t clusterOffset = f32_info.FAT1Address + clusterNum * sizeof(uint32_t);
  printf("newclusteroffset: 0x%x\n", clusterOffset);
  // go to that cluster and write it as in use
  fseek(fp, clusterOffset, SEEK_SET);
  fwrite(&nextCluster, sizeof(uint32_t), 1, fp);
  fflush(fp);
}

// // fills dir struct w/ root directory information
// void readRootDir()
// {
//   fseek(fp, f32_info.RootDir, SEEK_SET);

//   size_t bytesRead = fread(dir, (sizeof( struct DirectoryEntry) * 16), 1, fp);
//   if (bytesRead == 1) 
//   {
//     printf("successful fread\n");
//   }
//   else if (feof(fp))
//   {
//     printf("EOF, no more data\n");
//   }
//   else
//   {
//     // could do something like i--; to make it reread it?
//     perror("ERR: ");
//   }
//   // reset fp
//   fseek(fp, 0, SEEK_SET);
// }

// reads in current_working_dir location, and fills dir struct w/ info
void readHardCodedDir()
{
  FILE *newFP = NULL;
  newFP = fopen("fat32.img", "r+");
  printf("current_working_dir %d 0x%x\n", current_working_dir, current_working_dir);
  printf("Hard coded offset: 0x100400\n");
  fseek(newFP, 0x100400, SEEK_SET);
  // printf("seeking: %p\n", *fp);
  // fill our dir struct w/ files from currentDir
  size_t bytesRead = fread(dir, (sizeof( struct DirectoryEntry) * 16), 1, newFP);
  if (bytesRead == 1) 
  {
    // printf("successful fread\n");
  }
  else if (feof(newFP))
  {
    printf("EOF, no more data\n");
  }
  else if (ferror(newFP))
  {
    // could do something like i--; to make it reread it?
    perror("ERR: ");
    clearerr(newFP);
  }
  // reset fp
  fseek(newFP, 0, SEEK_SET);

  fclose(newFP);
}

void readDir()
{
  // printf("current_working_dir %d 0x%x\n", current_working_dir, current_working_dir);
  fseek(fp, offset, SEEK_SET);
  // printf("seeking: %p\n", *fp);
  // fill our dir struct w/ files from currentDir
  size_t bytesRead = fread(dir, (sizeof( struct DirectoryEntry) * 16), 1, fp);
  if (bytesRead == 1) 
  {
    // printf("successful fread\n");
  }
  else if (feof(fp))
  {
    printf("EOF, no more data\n");
  }
  else if (ferror(fp))
  {
    // could do something like i--; to make it reread it?
    perror("ERR: ");
    clearerr(fp);
  }
  // reset fp
  fseek(fp, 0, SEEK_SET);
}

// Listdir reads in current_working_dir (using helper), and then prints out
// the directories contents
void listDir()
{
  int nextBlk = 0;
  // read in current directory
  do
  {
    readDir();
    // loop through dir struct
    for (int i = 0; i < 16; i++)
    {
      // allow files and directories to be listed
      if (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20 )
      {
        // printf("first char hex val: 0x%x\n", dir[i].DIR_Name[0]);
        if ( dir[i].DIR_Name[0] != (char)0xe5)
        {
          // print char by char the dir names
          for (int j = 0; j < 11; j++)
          {
            // if our first char is 0x05 print it as the deleted flag
            // as that 0x05 is the placeholder for 0xe5
            if ( dir[i].DIR_Name[0] == 0x05 )
            {
              printf("%c", 0xe5);
            }
            else
            {
              printf("%c", dir[i].DIR_Name[j]);
            }
          }
          printf("\n");
          // printf("dir attributes: %x\n", dir[i].DIR_Attr);
        }
      }
    }
    nextBlk = NextLB(current_working_dir);
    if ( nextBlk >= 0 ) 
    {
      offset = LBAToOffset(nextBlk);
    }

  }
  while (nextBlk >= 0);
}

// void getParentDir()
// {
//   for (int i = 0; i < 16; i++)
//   {
//     // printf("dirname: %s\n", dir[i].DIR_Name);
//     // printf("dirnameAtt: %x\n", dir[i].DIR_Attr);
//     // printf("strncmp res: %d\n\n", strncmp(dir[i].DIR_Name, "..", 2));
//     if ( ( strncmp(dir[i].DIR_Name, "..", 2) == 0 ) && dir[i].DIR_Attr == 0x10 )
//     {
//       uint16_t clusterHigh = dir[i].DIR_FirstClusterHigh;
//       uint16_t clusterLow = dir[i].DIR_FirstClusterLow;
//       // we combine the high and low clusters by typecasting them to
//       // 32 bit unsigned ints, bitshifting left the high cluster by 16
//       // and adding the typecasted clusterLow to that.
//       uint32_t combinedCluster = ( (uint32_t)clusterHigh << 16) + ((uint32_t)clusterLow);
//       printf("combined cluster: %u 0x%x\n", combinedCluster, combinedCluster);

//       // Root dir is above us
//       if (combinedCluster == 0)
//       {
//         offset = f32_info.RootDir;
//       }
//       else
//       {
//         // change our dir to one above
//         offset = LBAToOffset(combinedCluster);
//         // printf("currentWorkindDir = %d 0x%x\n", current_working_dir, current_working_dir);
//       }
//       return; // found parent dir
//     }
//   }

//   printf("error: cannot move above parent dir\n");
// }

void getSubdir(char *name)
{
  // we have to make sure we convert our user input to something readable by our filesys

  readDir();

  // printf("looking for file\n");
  // we switch to the directory passed in token[1]
  for (int i = 0; i < 16; i++)
  {
    // if the directory names match and the entry is a directory
    // printf("expandedname: %s\n", expanded_name);
    // printf("dirname: %s\n", dir[i].DIR_Name);
    // printf("dirnameAtt: %x\n", dir[i].DIR_Attr);
    // printf("strncmp res: %d\n\n", strncmp(dir[i].DIR_Name, expanded_name, 11));
    if ( (compareUserString(name, dir[i].DIR_Name)) && dir[i].DIR_Attr == 0x10 )
    {
      uint16_t clusterHigh = dir[i].DIR_FirstClusterHigh;
      uint16_t clusterLow = dir[i].DIR_FirstClusterLow;
      uint32_t combinedCluster = ( (uint32_t)clusterHigh << 16) | ((uint32_t)clusterLow);
      // printf("clusterHigh: %d 0x%x\n", clusterHigh, clusterHigh);
      // printf("clusterLow: %d 0x%x\n", clusterLow, clusterLow);
      // printf("combined cluster: %u 0x%x\n", combinedCluster, combinedCluster);

      // update current working directory
      offset = LBAToOffset(combinedCluster);
      // printf("currentWorkindDir = 0x%x\n", current_working_dir);
      return;
    }
  }
  
  printf("err: dir not found %s\n", name);
}

// Finds the starting address of a block of data given the sector number (+ f32_info)
uint32_t LBAToOffset(int32_t sector)
{
  if ( sector == 0)
  {
    sector = 2;
  }
  return ( ((sector - 2) * f32_info.BPB_BytesPerSec) + (f32_info.BPB_BytesPerSec * f32_info.BPB_RsvdSecCnt) + (f32_info.BPB_NumFATS * f32_info.BPB_FATSz32 * f32_info.BPB_BytesPerSec));
}

// Given a logical block address, look up into the first fat and return the logical blk address of the block in the file
// if no further blocks then ret -1
int16_t NextLB( uint32_t sector)
{
  uint32_t FATAddress = f32_info.BPB_BytesPerSec * f32_info.BPB_RsvdSecCnt + (sector * 4);
  int16_t val;
  fseek(fp, FATAddress, SEEK_SET);
  fread( &val, 2, 1, fp);
  return val;
}

int compareUserString(char *userName, char *dirName)
{
  // we have to make sure we convert our user input to something readable by our filesys

  if( (strncmp(userName, "..", 2)) == 0 && (strncmp(userName, dirName, 2) == 0) )
  {
    return 1;
  }
  else if( (strncmp(userName, ".", 1)) == 0 && (strncmp(userName, dirName, 1) == 0) )
  {
    return 1;
  }

  char expanded_name[12];
  memset( expanded_name, ' ', 12); 

  char *tok = strtok(userName, ".");
  strncpy( expanded_name, tok, strlen(tok));
  tok = strtok(NULL, ".");

  if (tok)
  {
    strncpy( (char*)(expanded_name+8), tok, strlen(tok));
  }
  expanded_name[11] = '\0';

  return (strncmp(expanded_name, dirName, 11) == 0);
}

void convertNameToUpper(char *userName)
{
  memset( expanded_user_name, ' ', 12); 

  char *tok = strtok(userName, ".");
  strncpy( expanded_user_name, tok, strlen(tok));
  tok = strtok(NULL, ".");

  if (tok)
  {
    strncpy( (char*)(expanded_user_name+8), tok, strlen(tok));
  }
  expanded_user_name[11] = '\0';
}

// void convertNewNameToExpanded(char *name)
// {
//   // we have to make sure we convert our user input to something readable by our filesys
//   memset( new_expanded_name, ' ', 12); 

//   char *tok = strtok(name, ".");
//   strncpy( new_expanded_name, tok, strlen(tok));
//   tok = strtok(NULL, ".");

//   if (tok)
//   {
//     strncpy( (char*)(new_expanded_name+8), tok, strlen(tok));
//   }
//   new_expanded_name[11] = '\0';
// }


/* Useful file converitng code

// printf("imgname: %s\n", imgname);

// char expanded_name[12];
// memset( expanded_name, ' ', 12);
// // printf("expanded memset: %s\n", expanded_name);

// char *tok = strtok(imgname, ".");
// strncpy( expanded_name, tok, strlen(tok));
// tok = strtok(NULL, ".");

// if (tok)
// {
//   strncpy( (char*)(expanded_name+8), tok, strlen(tok));
// }
// expanded_name[11] = '\0';

// printf("expanded name: %s\n", expanded_name);

*/