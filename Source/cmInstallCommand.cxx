/*=========================================================================

  Program:   CMake - Cross-Platform Makefile Generator
  Module:    $RCSfile$
  Language:  C++
  Date:      $Date$
  Version:   $Revision$

  Copyright (c) 2002 Kitware, Inc., Insight Consortium.  All rights reserved.
  See Copyright.txt or http://www.cmake.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notices for more information.

=========================================================================*/
#include "cmInstallCommand.h"

#include "cmInstallFilesGenerator.h"
#include "cmInstallScriptGenerator.h"
#include "cmInstallTargetGenerator.h"

// cmInstallCommand
bool cmInstallCommand::InitialPass(std::vector<std::string> const& args)
{
  // Allow calling with no arguments so that arguments may be built up
  // using a variable that may be left empty.
  if(args.empty())
    {
    return true;
    }

  // Switch among the command modes.
  if(args[0] == "SCRIPT")
    {
    return this->HandleScriptMode(args);
    }
  else if(args[0] == "TARGETS")
    {
    return this->HandleTargetsMode(args);
    }
  else if(args[0] == "FILES")
    {
    return this->HandleFilesMode(args);
    }
  else if(args[0] == "PROGRAMS")
    {
    return this->HandleFilesMode(args);
    }

  // Unknown mode.
  cmStdString e = "called with unknown mode ";
  e += args[0];
  this->SetError(e.c_str());
  return false;
}

//----------------------------------------------------------------------------
bool cmInstallCommand::HandleScriptMode(std::vector<std::string> const& args)
{
  bool doing_script = false;
  for(size_t i=0; i < args.size(); ++i)
    {
    if(args[i] == "SCRIPT")
      {
      doing_script = true;
      }
    else if(doing_script)
      {
      doing_script = false;
      std::string script = args[i];
      if(!cmSystemTools::FileIsFullPath(script.c_str()))
        {
        script = m_Makefile->GetCurrentDirectory();
        script += "/";
        script += args[i];
        }
      if(cmSystemTools::FileIsDirectory(script.c_str()))
        {
        this->SetError("given a directory as value of SCRIPT argument.");
        return false;
        }
      m_Makefile->AddInstallGenerator(
        new cmInstallScriptGenerator(script.c_str()));
      }
    }
  if(doing_script)
    {
    this->SetError("given no value for SCRIPT argument.");
    return false;
    }
  return true;
}

//----------------------------------------------------------------------------
bool cmInstallCommand::HandleTargetsMode(std::vector<std::string> const& args)
{
  // This is the TARGETS mode.
  bool doing_targets = true;
  bool doing_destination = false;
  bool library_settings = true;
  bool runtime_settings = true;
  std::vector<cmTarget*> targets;
  const char* library_destination = 0;
  const char* runtime_destination = 0;
  for(unsigned int i=1; i < args.size(); ++i)
    {
    if(args[i] == "DESTINATION")
      {
      // Switch to setting the destination property.
      doing_targets = false;
      doing_destination = true;
      }
    else if(args[i] == "LIBRARY")
      {
      // Switch to setting only library properties.
      doing_targets = false;
      doing_destination = false;
      library_settings = true;
      runtime_settings = false;
      }
    else if(args[i] == "RUNTIME")
      {
      // Switch to setting only runtime properties.
      doing_targets = false;
      doing_destination = false;
      library_settings = false;
      runtime_settings = true;
      }
    else if(doing_targets)
      {
      // Lookup this target in the current directory.
      if(cmTarget* target = m_Makefile->FindTarget(args[i].c_str()))
        {
        // Found the target.  Check its type.
        if(target->GetType() != cmTarget::EXECUTABLE &&
           target->GetType() != cmTarget::STATIC_LIBRARY &&
           target->GetType() != cmTarget::SHARED_LIBRARY &&
           target->GetType() != cmTarget::MODULE_LIBRARY)
          {
          cmOStringStream e;
          e << "TARGETS given target \"" << args[i]
            << "\" which is not an executable, library, or module.";
          this->SetError(e.str().c_str());
          return false;
          }

        // Store the target in the list to be installed.
        targets.push_back(target);
        }
      else
        {
        // Did not find the target.
        cmOStringStream e;
        e << "TARGETS given target \"" << args[i]
          << "\" which does not exist in this directory.";
        this->SetError(e.str().c_str());
        return false;
        }
      }
    else if(doing_destination)
      {
      // Set the destination in the active set(s) of properties.
      if(library_settings)
        {
        library_destination = args[i].c_str();
        }
      if(runtime_settings)
        {
        runtime_destination = args[i].c_str();
        }
      doing_destination = false;
      }
    else
      {
      // Unknown argument.
      cmOStringStream e;
      e << "TARGETS given unknown argument \"" << args[i] << "\".";
      this->SetError(e.str().c_str());
      return false;
      }
    }

  // Check if there is something to do.
  if(targets.empty())
    {
    return true;
    }
  if(!library_destination && !runtime_destination)
    {
    this->SetError("TARGETS given no DESTINATION!");
    return false;
    }

  // Compute destination paths.
  std::string library_dest;
  std::string runtime_dest;
  this->ComputeDestination(library_destination, library_dest);
  this->ComputeDestination(runtime_destination, runtime_dest);

  // Generate install script code to install the given targets.
  for(std::vector<cmTarget*>::iterator ti = targets.begin();
      ti != targets.end(); ++ti)
    {
    // Handle each target type.
    cmTarget& target = *(*ti);
    switch(target.GetType())
      {
      case cmTarget::SHARED_LIBRARY:
        {
        // Shared libraries are handled differently on DLL and non-DLL
        // platforms.  All windows platforms are DLL platforms
        // including cygwin.  Currently no other platform is a DLL
        // platform.
#if defined(_WIN32) || defined(__CYGWIN__)
        // This is a DLL platform.
        if(library_destination)
          {
          // The import library uses the LIBRARY properties.
          m_Makefile->AddInstallGenerator(
            new cmInstallTargetGenerator(target, library_dest.c_str(), true));
          }
        if(runtime_destination)
          {
          // The DLL uses the RUNTIME properties.
          m_Makefile->AddInstallGenerator(
            new cmInstallTargetGenerator(target, runtime_dest.c_str(), false));
          }
#else
        // This is a non-DLL platform.
        if(library_destination)
          {
          // The shared library uses the LIBRARY properties.
          m_Makefile->AddInstallGenerator(
            new cmInstallTargetGenerator(target, library_dest.c_str()));
          }
#endif
        }
        break;
      case cmTarget::STATIC_LIBRARY:
      case cmTarget::MODULE_LIBRARY:
        {
        // Static libraries and modules use LIBRARY properties.
        if(library_destination)
          {
          m_Makefile->AddInstallGenerator(
            new cmInstallTargetGenerator(target, library_dest.c_str()));
          }
        else
          {
          cmOStringStream e;
          e << "TARGETS given no LIBRARY DESTINATION for ";
          if(target.GetType() == cmTarget::STATIC_LIBRARY)
            {
            e << "static library";
            }
          else
            {
            e << "module";
            }
          e << " target \"" << target.GetName() << "\".";
          this->SetError(e.str().c_str());
          return false;
          }
        }
        break;
      case cmTarget::EXECUTABLE:
        {
        // Executables use the RUNTIME properties.
        if(runtime_destination)
          {
          m_Makefile->AddInstallGenerator(
            new cmInstallTargetGenerator(target, runtime_dest.c_str()));
          }
        else
          {
          cmOStringStream e;
          e << "TARGETS given no RUNTIME DESTINATION for executable target \""
            << target.GetName() << "\".";
          this->SetError(e.str().c_str());
          return false;
          }
        }
        break;
      default:
        // This should never happen due to the above type check.
        // Ignore the case.
        break;
      }
    }

  return true;
}

//----------------------------------------------------------------------------
bool cmInstallCommand::HandleFilesMode(std::vector<std::string> const& args)
{
  // This is the FILES mode.
  bool programs = (args[0] == "PROGRAMS");
  bool doing_files = true;
  bool doing_destination = false;
  bool doing_permissions = false;
  bool doing_rename = false;
  std::vector<std::string> files;
  const char* destination = 0;
  std::string rename;
  std::string permissions;
  for(unsigned int i=1; i < args.size(); ++i)
    {
    if(args[i] == "DESTINATION")
      {
      // Switch to setting the destination property.
      doing_files = false;
      doing_destination = true;
      doing_permissions = false;
      doing_rename = false;
      }
    else if(args[i] == "PERMISSIONS")
      {
      // Switch to setting the permissions property.
      doing_files = false;
      doing_destination = false;
      doing_permissions = true;
      doing_rename = false;
      }
    else if(args[i] == "RENAME")
      {
      // Switch to setting the rename property.
      doing_files = false;
      doing_destination = false;
      doing_permissions = false;
      doing_rename = true;
      }
    else if(doing_files)
      {
      // Convert this file to a full path.
      std::string file = args[i];
      if(!cmSystemTools::FileIsFullPath(file.c_str()))
        {
        file = m_Makefile->GetCurrentDirectory();
        file += "/";
        file += args[i];
        }

      // Make sure the file is not a directory.
      if(cmSystemTools::FileIsDirectory(file.c_str()))
        {
        cmOStringStream e;
        e << args[0] << " given directory \"" << args[i] << "\" to install.";
        this->SetError(e.str().c_str());
        return false;
        }

      // Store the file for installation.
      files.push_back(file);
      }
    else if(doing_destination)
      {
      destination = args[i].c_str();
      doing_destination = false;
      }
    else if(doing_permissions)
      {
      // Check the requested permission.
      if(!this->CheckPermissions(args[i], permissions))
        {
        cmOStringStream e;
        e << args[0] << " given invalid permission \""
          << args[i] << "\".";
        this->SetError(e.str().c_str());
        return false;
        }
      }
    else if(doing_rename)
      {
      rename = args[i];
      doing_rename = false;
      }
    else
      {
      // Unknown argument.
      cmOStringStream e;
      e << args[0] << " given unknown argument \"" << args[i] << "\".";
      this->SetError(e.str().c_str());
      return false;
      }
    }

  // Check if there is something to do.
  if(files.empty())
    {
    return true;
    }
  if(!destination)
    {
    // A destination is required.
    cmOStringStream e;
    e << args[0] << " given no DESTINATION!";
    this->SetError(e.str().c_str());
    return false;
    }
  if(!rename.empty() && files.size() > 1)
    {
    // The rename option works only with one file.
    cmOStringStream e;
    e << args[0] << " given RENAME option with more than one file.";
    this->SetError(e.str().c_str());
    return false;
    }

  // Compute destination path.
  std::string dest;
  this->ComputeDestination(destination, dest);

  // Create the files install generator.
  m_Makefile->AddInstallGenerator(
    new cmInstallFilesGenerator(files, dest.c_str(), programs,
                                permissions.c_str(), rename.c_str()));

  return true;
}

//----------------------------------------------------------------------------
void cmInstallCommand::ComputeDestination(const char* destination,
                                          std::string& dest)
{
  if(destination)
    {
    if(cmSystemTools::FileIsFullPath(destination))
      {
      // Full paths are absolute.
      dest = destination;
      }
    else
      {
      // Relative paths are treated with respect to the installation prefix.
      dest = "${CMAKE_INSTALL_PREFIX}/";
      dest += destination;
      }

    // Format the path nicely.  Note this also removes trailing
    // slashes.
    cmSystemTools::ConvertToUnixSlashes(dest);
    }
  else
    {
    dest = "";
    }
}

//----------------------------------------------------------------------------
bool cmInstallCommand::CheckPermissions(std::string const& arg,
                                        std::string& permissions)
{
  // Table of valid permissions.
  const char* table[] =
    {
      "OWNER_READ", "OWNER_WRITE", "OWNER_EXECUTE",
      "GROUP_READ", "GROUP_WRITE", "GROUP_EXECUTE",
      "WORLD_READ", "WORLD_WRITE", "WORLD_EXECUTE",
      "SETUID", "SETGID", 0
    };

  // Check the permission against the table.
  for(const char** valid = table; *valid; ++valid)
    {
    if(arg == *valid)
      {
      // This is a valid permission.
      permissions += " ";
      permissions += arg;
      return true;
      }
    }

  // This is not a valid permission.
  return false;
}
