﻿// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.
// See the LICENSE file in the project root for more information.

using System;
using System.Collections.Generic;
using System.Reflection;

using Internal.TypeSystem;

using Debug = System.Diagnostics.Debug;

namespace ILCompiler
{
    public abstract class CompilationModuleGroup
    {
        /// <summary>
        /// Gets the synthetic assembly that holds types generated by the compiler as part of the compilation process.
        /// Types and members that declare this module as their owning module are always generated.
        /// </summary>
        public ModuleDesc GeneratedAssembly { get; }

        public CompilationModuleGroup(TypeSystemContext context)
        {
            GeneratedAssembly = new CompilerGeneratedAssembly(context);
        }

        /// <summary>
        /// If true, "type" is in the set of input assemblies being compiled
        /// </summary>
        public abstract bool ContainsType(TypeDesc type);
        /// <summary>
        /// If true, "method" is in the set of input assemblies being compiled
        /// </summary>
        public abstract bool ContainsMethodBody(MethodDesc method);
        /// <summary>
        /// If true, the generic dictionary of "method" is in the set of input assemblies being compiled
        /// </summary>
        public abstract bool ContainsMethodDictionary(MethodDesc method);
        /// <summary>
        /// If true, "type" is exported by the set of input assemblies being compiled
        /// </summary>
        public abstract bool ExportsType(TypeDesc type);
        /// <summary>
        /// If true, "method" is exported by the set of input assemblies being compiled
        /// </summary>
        public abstract bool ExportsMethod(MethodDesc method);
        /// <summary>
        /// If true, the generic dictionary of "method" is exported by the set of input assemblies being compiled
        /// </summary>
        public abstract bool ExportsMethodDictionary(MethodDesc method);
        /// <summary>
        /// If true, all code is compiled into a single module
        /// </summary>
        public abstract bool IsSingleFileCompilation { get; }
        /// <summary>
        /// If true, the full type should be generated. This occurs in situations where the type is 
        /// shared between modules (generics, parameterized types), or the type lives in a different module
        /// and therefore needs a full VTable
        /// </summary>
        public abstract bool ShouldProduceFullVTable(TypeDesc type);
        /// <summary>
        /// If true, the necessary type should be promoted to a full type should be generated. 
        /// </summary>
        public abstract bool ShouldPromoteToFullType(TypeDesc type);
        /// <summary>
        /// If true, the type will not be linked into the same module as the current compilation and therefore
        /// accessed through the target platform's import mechanism (ie, Import Address Table on Windows)
        /// </summary>
        public abstract bool ShouldReferenceThroughImportTable(TypeDesc type);

        /// <summary>
        /// If true, there may be type system constructs that will not be linked into the same module as the current compilation and therefore
        /// accessed through the target platform's import mechanism (ie, Import Address Table on Windows)
        /// </summary>
        public abstract bool CanHaveReferenceThroughImportTable { get; }

        private class CompilerGeneratedAssembly : ModuleDesc, IAssemblyDesc
        {
            private MetadataType _globalModuleType;

            public CompilerGeneratedAssembly(TypeSystemContext context)
                : base(context)
            {
                _globalModuleType = new CompilerGeneratedType(this, "<Module>");
            }

            public override IEnumerable<MetadataType> GetAllTypes()
            {
                return Array.Empty<MetadataType>();
            }

            public override MetadataType GetGlobalModuleType()
            {
                return _globalModuleType;
            }

            public AssemblyName GetName()
            {
                return new AssemblyName("System.Private.CompilerGenerated");
            }

            public override MetadataType GetType(string nameSpace, string name, bool throwIfNotFound = true)
            {
                Debug.Fail("Resolving a TypeRef in the compiler generated assembly?");

                if (throwIfNotFound)
                    ThrowHelper.ThrowTypeLoadException(nameSpace, name, this);

                return null;
            }
        }
    }
}
