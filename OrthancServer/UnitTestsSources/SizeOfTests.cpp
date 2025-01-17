/**
 * Orthanc - A Lightweight, RESTful DICOM Store
 * Copyright (C) 2012-2016 Sebastien Jodogne, Medical Physics
 * Department, University Hospital of Liege, Belgium
 * Copyright (C) 2017-2023 Osimis S.A., Belgium
 * Copyright (C) 2024-2025 Orthanc Team SRL, Belgium
 * Copyright (C) 2021-2025 Sebastien Jodogne, ICTEAM UCLouvain, Belgium
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 **/


#include "PrecompiledHeadersUnitTests.h"
#include <gtest/gtest.h>

#include "../../OrthancFramework/Sources/Cache/ICachePageProvider.h"
#include "../../OrthancFramework/Sources/Cache/ICacheable.h"
#include "../../OrthancFramework/Sources/Cache/LeastRecentlyUsedIndex.h"
#include "../../OrthancFramework/Sources/Cache/MemoryCache.h"
#include "../../OrthancFramework/Sources/Cache/MemoryObjectCache.h"
#include "../../OrthancFramework/Sources/Cache/MemoryStringCache.h"
#include "../../OrthancFramework/Sources/Cache/SharedArchive.h"
#include "../../OrthancFramework/Sources/ChunkedBuffer.h"
#include "../../OrthancFramework/Sources/Compatibility.h"
#include "../../OrthancFramework/Sources/Compression/DeflateBaseCompressor.h"
#include "../../OrthancFramework/Sources/Compression/GzipCompressor.h"
#include "../../OrthancFramework/Sources/Compression/HierarchicalZipWriter.h"
#include "../../OrthancFramework/Sources/Compression/IBufferCompressor.h"
#include "../../OrthancFramework/Sources/Compression/ZipReader.h"
#include "../../OrthancFramework/Sources/Compression/ZipWriter.h"
#include "../../OrthancFramework/Sources/Compression/ZlibCompressor.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomArray.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomElement.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomImageInformation.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomInstanceHasher.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomIntegerPixelAccessor.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomMap.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomStreamReader.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomTag.h"
#include "../../OrthancFramework/Sources/DicomFormat/DicomValue.h"
#include "../../OrthancFramework/Sources/DicomFormat/StreamBlockReader.h"
#include "../../OrthancFramework/Sources/DicomNetworking/DicomAssociation.h"
#include "../../OrthancFramework/Sources/DicomNetworking/DicomAssociationParameters.h"
#include "../../OrthancFramework/Sources/DicomNetworking/DicomControlUserConnection.h"
#include "../../OrthancFramework/Sources/DicomNetworking/DicomFindAnswers.h"
#include "../../OrthancFramework/Sources/DicomNetworking/DicomServer.h"
#include "../../OrthancFramework/Sources/DicomNetworking/DicomStoreUserConnection.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IApplicationEntityFilter.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IFindRequestHandler.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IFindRequestHandlerFactory.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IGetRequestHandler.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IGetRequestHandlerFactory.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IMoveRequestHandler.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IMoveRequestHandlerFactory.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IStorageCommitmentRequestHandler.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IStorageCommitmentRequestHandlerFactory.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IStoreRequestHandler.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IStoreRequestHandlerFactory.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IWorklistRequestHandler.h"
#include "../../OrthancFramework/Sources/DicomNetworking/IWorklistRequestHandlerFactory.h"
#include "../../OrthancFramework/Sources/DicomNetworking/Internals/CommandDispatcher.h"
#include "../../OrthancFramework/Sources/DicomNetworking/Internals/DicomTls.h"
#include "../../OrthancFramework/Sources/DicomNetworking/Internals/FindScp.h"
#include "../../OrthancFramework/Sources/DicomNetworking/Internals/GetScp.h"
#include "../../OrthancFramework/Sources/DicomNetworking/Internals/MoveScp.h"
#include "../../OrthancFramework/Sources/DicomNetworking/Internals/StoreScp.h"
#include "../../OrthancFramework/Sources/DicomNetworking/NetworkingCompatibility.h"
#include "../../OrthancFramework/Sources/DicomNetworking/RemoteModalityParameters.h"
#include "../../OrthancFramework/Sources/DicomNetworking/TimeoutDicomConnectionManager.h"
#include "../../OrthancFramework/Sources/DicomParsing/DcmtkTranscoder.h"
#include "../../OrthancFramework/Sources/DicomParsing/DicomDirWriter.h"
#include "../../OrthancFramework/Sources/DicomParsing/DicomModification.h"
#include "../../OrthancFramework/Sources/DicomParsing/DicomWebJsonVisitor.h"
#include "../../OrthancFramework/Sources/DicomParsing/FromDcmtkBridge.h"
#include "../../OrthancFramework/Sources/DicomParsing/IDicomTranscoder.h"
#include "../../OrthancFramework/Sources/DicomParsing/ITagVisitor.h"
#include "../../OrthancFramework/Sources/DicomParsing/Internals/DicomFrameIndex.h"
#include "../../OrthancFramework/Sources/DicomParsing/Internals/DicomImageDecoder.h"
#include "../../OrthancFramework/Sources/DicomParsing/MemoryBufferTranscoder.h"
#include "../../OrthancFramework/Sources/DicomParsing/ParsedDicomCache.h"
#include "../../OrthancFramework/Sources/DicomParsing/ParsedDicomDir.h"
#include "../../OrthancFramework/Sources/DicomParsing/ParsedDicomFile.h"
#include "../../OrthancFramework/Sources/DicomParsing/ToDcmtkBridge.h"
#include "../../OrthancFramework/Sources/Endianness.h"
#include "../../OrthancFramework/Sources/EnumerationDictionary.h"
#include "../../OrthancFramework/Sources/Enumerations.h"
#include "../../OrthancFramework/Sources/FileBuffer.h"
#include "../../OrthancFramework/Sources/FileStorage/FileInfo.h"
#include "../../OrthancFramework/Sources/FileStorage/FilesystemStorage.h"
#include "../../OrthancFramework/Sources/FileStorage/IStorageArea.h"
#include "../../OrthancFramework/Sources/FileStorage/MemoryStorageArea.h"
#include "../../OrthancFramework/Sources/FileStorage/StorageAccessor.h"
#include "../../OrthancFramework/Sources/FileStorage/StorageCache.h"
#include "../../OrthancFramework/Sources/HttpClient.h"
#include "../../OrthancFramework/Sources/HttpServer/BufferHttpSender.h"
#include "../../OrthancFramework/Sources/HttpServer/FilesystemHttpHandler.h"
#include "../../OrthancFramework/Sources/HttpServer/FilesystemHttpSender.h"
#include "../../OrthancFramework/Sources/HttpServer/HttpContentNegociation.h"
#include "../../OrthancFramework/Sources/HttpServer/HttpFileSender.h"
#include "../../OrthancFramework/Sources/HttpServer/HttpOutput.h"
#include "../../OrthancFramework/Sources/HttpServer/HttpServer.h"
#include "../../OrthancFramework/Sources/HttpServer/HttpStreamTranscoder.h"
#include "../../OrthancFramework/Sources/HttpServer/HttpToolbox.h"
#include "../../OrthancFramework/Sources/HttpServer/IHttpHandler.h"
#include "../../OrthancFramework/Sources/HttpServer/IHttpOutputStream.h"
#include "../../OrthancFramework/Sources/HttpServer/IHttpStreamAnswer.h"
#include "../../OrthancFramework/Sources/HttpServer/IIncomingHttpRequestFilter.h"
#include "../../OrthancFramework/Sources/HttpServer/IWebDavBucket.h"
#include "../../OrthancFramework/Sources/HttpServer/MultipartStreamReader.h"
#include "../../OrthancFramework/Sources/HttpServer/StringHttpOutput.h"
#include "../../OrthancFramework/Sources/HttpServer/StringMatcher.h"
#include "../../OrthancFramework/Sources/HttpServer/WebDavStorage.h"
#include "../../OrthancFramework/Sources/IDynamicObject.h"
#include "../../OrthancFramework/Sources/IMemoryBuffer.h"
#include "../../OrthancFramework/Sources/Images/Font.h"
#include "../../OrthancFramework/Sources/Images/FontRegistry.h"
#include "../../OrthancFramework/Sources/Images/IImageWriter.h"
#include "../../OrthancFramework/Sources/Images/Image.h"
#include "../../OrthancFramework/Sources/Images/ImageAccessor.h"
#include "../../OrthancFramework/Sources/Images/ImageBuffer.h"
#include "../../OrthancFramework/Sources/Images/ImageProcessing.h"
#include "../../OrthancFramework/Sources/Images/ImageTraits.h"
#include "../../OrthancFramework/Sources/Images/JpegReader.h"
#include "../../OrthancFramework/Sources/Images/JpegWriter.h"
#include "../../OrthancFramework/Sources/Images/NumpyWriter.h"
#include "../../OrthancFramework/Sources/Images/PamReader.h"
#include "../../OrthancFramework/Sources/Images/PamWriter.h"
#include "../../OrthancFramework/Sources/Images/PixelTraits.h"
#include "../../OrthancFramework/Sources/Images/PngReader.h"
#include "../../OrthancFramework/Sources/Images/PngWriter.h"
#include "../../OrthancFramework/Sources/JobsEngine/GenericJobUnserializer.h"
#include "../../OrthancFramework/Sources/JobsEngine/IJob.h"
#include "../../OrthancFramework/Sources/JobsEngine/IJobUnserializer.h"
#include "../../OrthancFramework/Sources/JobsEngine/JobInfo.h"
#include "../../OrthancFramework/Sources/JobsEngine/JobStatus.h"
#include "../../OrthancFramework/Sources/JobsEngine/JobStepResult.h"
#include "../../OrthancFramework/Sources/JobsEngine/JobsEngine.h"
#include "../../OrthancFramework/Sources/JobsEngine/JobsRegistry.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/IJobOperation.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/IJobOperationValue.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/JobOperationValues.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/LogJobOperation.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/NullOperationValue.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/SequenceOfOperationsJob.h"
#include "../../OrthancFramework/Sources/JobsEngine/Operations/StringOperationValue.h"
#include "../../OrthancFramework/Sources/JobsEngine/SetOfCommandsJob.h"
#include "../../OrthancFramework/Sources/JobsEngine/SetOfInstancesJob.h"
#include "../../OrthancFramework/Sources/Logging.h"
#include "../../OrthancFramework/Sources/Lua/LuaContext.h"
#include "../../OrthancFramework/Sources/Lua/LuaFunctionCall.h"
#include "../../OrthancFramework/Sources/MallocMemoryBuffer.h"
#include "../../OrthancFramework/Sources/MetricsRegistry.h"
#include "../../OrthancFramework/Sources/MultiThreading/IRunnableBySteps.h"
#include "../../OrthancFramework/Sources/MultiThreading/RunnableWorkersPool.h"
#include "../../OrthancFramework/Sources/MultiThreading/Semaphore.h"
#include "../../OrthancFramework/Sources/MultiThreading/SharedMessageQueue.h"
#include "../../OrthancFramework/Sources/OrthancException.h"
#include "../../OrthancFramework/Sources/OrthancFramework.h"
#include "../../OrthancFramework/Sources/RestApi/RestApi.h"
#include "../../OrthancFramework/Sources/RestApi/RestApiCall.h"
#include "../../OrthancFramework/Sources/RestApi/RestApiCallDocumentation.h"
#include "../../OrthancFramework/Sources/RestApi/RestApiDeleteCall.h"
#include "../../OrthancFramework/Sources/RestApi/RestApiGetCall.h"
#include "../../OrthancFramework/Sources/RestApi/RestApiHierarchy.h"
#include "../../OrthancFramework/Sources/RestApi/RestApiOutput.h"
#include "../../OrthancFramework/Sources/RestApi/RestApiPath.h"
#include "../../OrthancFramework/Sources/RestApi/RestApiPostCall.h"
#include "../../OrthancFramework/Sources/RestApi/RestApiPutCall.h"
#include "../../OrthancFramework/Sources/SQLite/Connection.h"
#include "../../OrthancFramework/Sources/SQLite/FunctionContext.h"
#include "../../OrthancFramework/Sources/SQLite/IScalarFunction.h"
#include "../../OrthancFramework/Sources/SQLite/ITransaction.h"
#include "../../OrthancFramework/Sources/SQLite/NonCopyable.h"
#include "../../OrthancFramework/Sources/SQLite/OrthancSQLiteException.h"
#include "../../OrthancFramework/Sources/SQLite/SQLiteTypes.h"
#include "../../OrthancFramework/Sources/SQLite/Statement.h"
#include "../../OrthancFramework/Sources/SQLite/StatementId.h"
#include "../../OrthancFramework/Sources/SQLite/StatementReference.h"
#include "../../OrthancFramework/Sources/SQLite/Transaction.h"
#include "../../OrthancFramework/Sources/SerializationToolbox.h"
#include "../../OrthancFramework/Sources/SharedLibrary.h"
#include "../../OrthancFramework/Sources/StringMemoryBuffer.h"
#include "../../OrthancFramework/Sources/SystemToolbox.h"
#include "../../OrthancFramework/Sources/TemporaryFile.h"
#include "../../OrthancFramework/Sources/Toolbox.h"
#include "../../OrthancFramework/Sources/WebServiceParameters.h"


TEST(OrthancFramework, SizeOf)
{
#include "SizeOfTests.impl.h"
}
