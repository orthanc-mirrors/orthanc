/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program. If not, see
 * <http://www.gnu.org/licenses/>.
 **/


#if ORTHANC_UNIT_TESTS_LINK_FRAMEWORK == 1
// Must be the first to be sure to use the Orthanc framework shared library
#  include <OrthancFramework.h>
#endif

#include <gtest/gtest.h>

#include "../Sources/FileStorage/FilesystemStorage.h"
#include "../Sources/FileStorage/PluginStorageAreaAdapter.h"
#include "../Sources/FileStorage/StorageAccessor.h"
#include "../Sources/FileStorage/StorageCache.h"
#include "../Sources/Logging.h"
#include "../Sources/OrthancException.h"
#include "../Sources/Toolbox.h"
#include "../Sources/SystemToolbox.h"

#include <ctype.h>


using namespace Orthanc;


static void StringToVector(std::vector<uint8_t>& v,
                           const std::string& s)
{
  v.resize(s.size());
  for (size_t i = 0; i < s.size(); i++)
    v[i] = s[i];
}


TEST(FilesystemStorage, Basic)
{
  FilesystemStorage s("UnitTestsStorage");

  std::string data = Toolbox::GenerateUuid();
  std::string uid = Toolbox::GenerateUuid();
  s.Create(uid.c_str(), &data[0], data.size(), FileContentType_Unknown);
  std::string d;
  {
    std::unique_ptr<IMemoryBuffer> buffer(s.ReadWhole(uid, FileContentType_Unknown));
    buffer->MoveToString(d);    
  }
  ASSERT_EQ(d.size(), data.size());
  ASSERT_FALSE(memcmp(&d[0], &data[0], data.size()));
  ASSERT_EQ(s.GetSize(uid), data.size());
  {
    std::unique_ptr<IMemoryBuffer> buffer2(s.ReadRange(uid, FileContentType_Unknown, 0, uid.size()));
    std::string d2;
    buffer2->MoveToString(d2);
    ASSERT_EQ(d, d2);
  }
}

TEST(FilesystemStorage, Basic2)
{
  FilesystemStorage s("UnitTestsStorage");

  std::vector<uint8_t> data;
  StringToVector(data, Toolbox::GenerateUuid());
  std::string uid = Toolbox::GenerateUuid();
  s.Create(uid.c_str(), &data[0], data.size(), FileContentType_Unknown);
  std::string d;
  {
    std::unique_ptr<IMemoryBuffer> buffer(s.ReadWhole(uid, FileContentType_Unknown));
    buffer->MoveToString(d);    
  }
  ASSERT_EQ(d.size(), data.size());
  ASSERT_FALSE(memcmp(&d[0], &data[0], data.size()));
  ASSERT_EQ(s.GetSize(uid), data.size());
  {
    std::unique_ptr<IMemoryBuffer> buffer2(s.ReadRange(uid, FileContentType_Unknown, 0, uid.size()));
    std::string d2;
    buffer2->MoveToString(d2);
    ASSERT_EQ(d, d2);
  }
}

TEST(FilesystemStorage, FileWithSameNameAsTopDirectory)
{
  FilesystemStorage s("UnitTestsStorageTop");
  s.Clear();

  std::vector<uint8_t> data;
  StringToVector(data, Toolbox::GenerateUuid());

  SystemToolbox::WriteFile("toto", "UnitTestsStorageTop/12");
  ASSERT_THROW(s.Create("12345678-1234-1234-1234-1234567890ab", &data[0], data.size(), FileContentType_Unknown), OrthancException);
  s.Clear();
}

TEST(FilesystemStorage, FileWithSameNameAsChildDirectory)
{
  FilesystemStorage s("UnitTestsStorageChild");
  s.Clear();

  std::vector<uint8_t> data;
  StringToVector(data, Toolbox::GenerateUuid());

  SystemToolbox::MakeDirectory("UnitTestsStorageChild/12");
  SystemToolbox::WriteFile("toto", "UnitTestsStorageChild/12/34");
  ASSERT_THROW(s.Create("12345678-1234-1234-1234-1234567890ab", &data[0], data.size(), FileContentType_Unknown), OrthancException);
  s.Clear();
}

TEST(FilesystemStorage, FileAlreadyExists)
{
  FilesystemStorage s("UnitTestsStorageFileAlreadyExists");
  s.Clear();

  std::vector<uint8_t> data;
  StringToVector(data, Toolbox::GenerateUuid());

  SystemToolbox::MakeDirectory("UnitTestsStorageFileAlreadyExists/12/34");
  SystemToolbox::WriteFile("toto", "UnitTestsStorageFileAlreadyExists/12/34/12345678-1234-1234-1234-1234567890ab");
  ASSERT_THROW(s.Create("12345678-1234-1234-1234-1234567890ab", &data[0], data.size(), FileContentType_Unknown), OrthancException);
  s.Clear();
}


TEST(FilesystemStorage, EndToEnd)
{
  FilesystemStorage s("UnitTestsStorage");
  s.Clear();

  std::list<std::string> u;
  for (unsigned int i = 0; i < 10; i++)
  {
    std::string t = Toolbox::GenerateUuid();
    std::string uid = Toolbox::GenerateUuid();
    s.Create(uid.c_str(), &t[0], t.size(), FileContentType_Unknown);
    u.push_back(uid);
  }

  std::set<std::string> ss;
  s.ListAllFiles(ss);
  ASSERT_EQ(10u, ss.size());
  
  unsigned int c = 0;
  for (std::list<std::string>::const_iterator
         i = u.begin(); i != u.end(); ++i, c++)
  {
    ASSERT_TRUE(ss.find(*i) != ss.end());
    if (c < 5)
      s.Remove(*i, FileContentType_Unknown);
  }

  s.ListAllFiles(ss);
  ASSERT_EQ(5u, ss.size());

  s.Clear();
  s.ListAllFiles(ss);
  ASSERT_EQ(0u, ss.size());
}


TEST(StorageAccessor, NoCompression)
{
  PluginStorageAreaAdapter s(new FilesystemStorage("UnitTestsStorage"));
  StorageCache cache;
  StorageAccessor accessor(s, cache);

  const std::string data = "Hello world";
  FileInfo info;
  accessor.Write(info, data.c_str(), data.size(), FileContentType_Dicom, CompressionType_None, true, NULL);

  std::string r;
  accessor.Read(r, info);

  ASSERT_EQ(data, r);
  ASSERT_EQ(CompressionType_None, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(11u, info.GetCompressedSize());
  ASSERT_EQ(FileContentType_Dicom, info.GetContentType());
  ASSERT_EQ("3e25960a79dbc69b674cd4ec67a72c62", info.GetUncompressedMD5());
  ASSERT_EQ(info.GetUncompressedMD5(), info.GetCompressedMD5());
}


TEST(StorageAccessor, Compression)
{
  PluginStorageAreaAdapter s(new FilesystemStorage("UnitTestsStorage"));
  StorageCache cache;
  StorageAccessor accessor(s, cache);

  const std::string data = "Hello world";
  FileInfo info;
  accessor.Write(info, data.c_str(), data.size(), FileContentType_Dicom, CompressionType_ZlibWithSize, true, NULL);

  std::string r;
  accessor.Read(r, info);

  ASSERT_EQ(data, r);
  ASSERT_EQ(CompressionType_ZlibWithSize, info.GetCompressionType());
  ASSERT_EQ(11u, info.GetUncompressedSize());
  ASSERT_EQ(FileContentType_Dicom, info.GetContentType());
  ASSERT_EQ("3e25960a79dbc69b674cd4ec67a72c62", info.GetUncompressedMD5());
  ASSERT_NE(info.GetUncompressedMD5(), info.GetCompressedMD5());
}


TEST(StorageAccessor, Mix)
{
  PluginStorageAreaAdapter s(new FilesystemStorage("UnitTestsStorage"));
  StorageCache cache;
  StorageAccessor accessor(s, cache);

  const std::string compressedData = "Hello";
  const std::string uncompressedData = "HelloWorld";

  FileInfo compressedInfo;
  accessor.Write(compressedInfo, compressedData.c_str(), compressedData.size(), FileContentType_Dicom, CompressionType_ZlibWithSize, false, NULL);

  std::string r;
  accessor.Read(r, compressedInfo);
  ASSERT_EQ(compressedData, r);

  FileInfo uncompressedInfo;
  accessor.Write(uncompressedInfo, uncompressedData.c_str(), uncompressedData.size(), FileContentType_Dicom, CompressionType_None, false, NULL);
  accessor.Read(r, uncompressedInfo);
  ASSERT_EQ(uncompressedData, r);
  ASSERT_NE(compressedData, r);

  /*
  // This test is too slow on Windows
  accessor.SetCompressionForNextOperations(CompressionType_ZlibWithSize);
  ASSERT_THROW(accessor.Read(r, uncompressedInfo.GetUuid(), FileContentType_Unknown), OrthancException);
  */
}


TEST(StorageAccessor, Range)
{
  {
    StorageAccessor::Range range;
    ASSERT_FALSE(range.HasStart());
    ASSERT_FALSE(range.HasEnd());
    ASSERT_THROW(range.GetStartInclusive(), OrthancException);
    ASSERT_THROW(range.GetEndInclusive(), OrthancException);
    ASSERT_EQ("bytes 0-99/100", range.FormatHttpContentRange(100));
    ASSERT_EQ("bytes 0-0/1", range.FormatHttpContentRange(1));
    ASSERT_THROW(range.FormatHttpContentRange(0), OrthancException);
    ASSERT_EQ(100u, range.GetContentLength(100));
    ASSERT_EQ(1u, range.GetContentLength(1));
    ASSERT_THROW(range.GetContentLength(0), OrthancException);

    range.SetStartInclusive(10);
    ASSERT_TRUE(range.HasStart());
    ASSERT_FALSE(range.HasEnd());
    ASSERT_EQ(10u, range.GetStartInclusive());
    ASSERT_THROW(range.GetEndInclusive(), OrthancException);
    ASSERT_EQ("bytes 10-99/100", range.FormatHttpContentRange(100));
    ASSERT_EQ("bytes 10-10/11", range.FormatHttpContentRange(11));
    ASSERT_THROW(range.FormatHttpContentRange(10), OrthancException);
    ASSERT_EQ(90u, range.GetContentLength(100));
    ASSERT_EQ(1u, range.GetContentLength(11));
    ASSERT_THROW(range.GetContentLength(10), OrthancException);

    range.SetEndInclusive(30);
    ASSERT_TRUE(range.HasStart());
    ASSERT_TRUE(range.HasEnd());
    ASSERT_EQ(10u, range.GetStartInclusive());
    ASSERT_EQ(30u, range.GetEndInclusive());
    ASSERT_EQ("bytes 10-30/100", range.FormatHttpContentRange(100));
    ASSERT_EQ("bytes 10-30/31", range.FormatHttpContentRange(31));
    ASSERT_THROW(range.FormatHttpContentRange(30), OrthancException);
    ASSERT_EQ(21u, range.GetContentLength(100));
    ASSERT_EQ(21u, range.GetContentLength(31));
    ASSERT_THROW(range.GetContentLength(30), OrthancException);
  }

  {
    StorageAccessor::Range range;
    range.SetEndInclusive(20);
    ASSERT_FALSE(range.HasStart());
    ASSERT_TRUE(range.HasEnd());
    ASSERT_THROW(range.GetStartInclusive(), OrthancException);
    ASSERT_EQ(20u, range.GetEndInclusive());
    ASSERT_EQ("bytes 0-20/100", range.FormatHttpContentRange(100));
    ASSERT_EQ("bytes 0-20/21", range.FormatHttpContentRange(21));
    ASSERT_THROW(range.FormatHttpContentRange(20), OrthancException);
    ASSERT_EQ(21u, range.GetContentLength(100));
    ASSERT_EQ(21u, range.GetContentLength(21));
    ASSERT_THROW(range.GetContentLength(20), OrthancException);
  }

  {
    StorageAccessor::Range range = StorageAccessor::Range::ParseHttpRange("bytes=1-30");
    ASSERT_TRUE(range.HasStart());
    ASSERT_TRUE(range.HasEnd());
    ASSERT_EQ(1u, range.GetStartInclusive());
    ASSERT_EQ(30u, range.GetEndInclusive());
    ASSERT_EQ("bytes 1-30/100", range.FormatHttpContentRange(100));
  }

  ASSERT_THROW(StorageAccessor::Range::ParseHttpRange("bytes="), OrthancException);
  ASSERT_THROW(StorageAccessor::Range::ParseHttpRange("bytes=-1-30"), OrthancException);
  ASSERT_THROW(StorageAccessor::Range::ParseHttpRange("bytes=100-30"), OrthancException);

  ASSERT_EQ("bytes 0-99/100", StorageAccessor::Range::ParseHttpRange("bytes=-").FormatHttpContentRange(100));
  ASSERT_EQ("bytes 0-10/100", StorageAccessor::Range::ParseHttpRange("bytes=-10").FormatHttpContentRange(100));
  ASSERT_EQ("bytes 10-99/100", StorageAccessor::Range::ParseHttpRange("bytes=10-").FormatHttpContentRange(100));

  {
    std::string s;
    StorageAccessor::Range::ParseHttpRange("bytes=1-2").Extract(s, "Hello");
    ASSERT_EQ("el", s);
    StorageAccessor::Range::ParseHttpRange("bytes=-2").Extract(s, "Hello");
    ASSERT_EQ("Hel", s);
    StorageAccessor::Range::ParseHttpRange("bytes=3-").Extract(s, "Hello");
    ASSERT_EQ("lo", s);
    StorageAccessor::Range::ParseHttpRange("bytes=-").Extract(s, "Hello");
    ASSERT_EQ("Hello", s);
    StorageAccessor::Range::ParseHttpRange("bytes=4-").Extract(s, "Hello");
    ASSERT_EQ("o", s);
    ASSERT_THROW(StorageAccessor::Range::ParseHttpRange("bytes=5-").Extract(s, "Hello"), OrthancException);
  }
}