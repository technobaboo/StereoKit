﻿using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Text;

namespace StereoKit
{
    public delegate void WithCallback<T>(ref T item);
    public class ECSManager
    {
        struct SystemInfo
        {
            public IComponentSystem system;
            public MethodInfo       addMethod;
        }
        
        static Dictionary<int, SystemInfo> systems  = new Dictionary<int, SystemInfo>();
        
        public static void Start()
        {
            // Look at classes in the current DLL. TODO: take a list of assemblies for user components
            Assembly assembly          = Assembly.GetExecutingAssembly();
            Type     rootComponentType = typeof(Component<>).GetGenericTypeDefinition();
            foreach (Type type in assembly.GetTypes())
            {
                if (type == rootComponentType)
                    continue;

                // Check if this type implements our root component type
                bool isComponent = type.GetInterfaces().Any(x =>
                  x.IsGenericType &&
                  x.GetGenericTypeDefinition() == rootComponentType);

                if (isComponent)
                {
                    // Make a system to manage that component type
                    Type systemType = typeof(ComponentSystem<>).MakeGenericType(type);
                    systems[type.GetHashCode()] = new SystemInfo {
                        system    = (IComponentSystem)Activator.CreateInstance(systemType),
                        addMethod = systemType.GetMethod("Add") 
                    };
                    Log.Write(LogLevel.Info, "Added component system for <~CYN>{0}<~clr>", type.Name);
                }
            }
        }

        public static void Shutdown()
        {
            foreach (SystemInfo info in systems.Values)
            {
                info.system.Shutdown();
            }
        }

        public static void Update()
        {
            Stopwatch stopWatch = new Stopwatch();
            stopWatch.Start();

            foreach (SystemInfo info in systems.Values)
            {
                info.system.Update();
            }

            stopWatch.Stop();
            Console.WriteLine("Update " + stopWatch.Elapsed.TotalMilliseconds + "ms");
        }

        public static ComponentId Add<T>(ref T item)
        {
            int hash = typeof(T).GetHashCode();
            return new ComponentId { 
                system = hash, 
                index  = (int)systems[hash].addMethod.Invoke(systems[hash].system, new object[]{item})
            };
        }

        static bool IsSubclassOfGeneric(Type generic, Type toCheck)
        {
            while (toCheck != null && toCheck != typeof(object))
            {
                Type curr = toCheck.IsGenericType ? toCheck.GetGenericTypeDefinition() : toCheck;
                if (generic == curr)
                {
                    return true;
                }
                toCheck = toCheck.BaseType;
            }
            return false;
        }

        public static void With<T>(ComponentId id, WithCallback<T> with) where T : struct, Component<T>
        {
            ((ComponentSystem<T>)systems[id.system].system).With(id, with);
        }

        public static void SetEnabled(ComponentId id, bool enabled)
        {
            systems[id.system].system.SetEnabled(id, enabled);
        }
    }
}
