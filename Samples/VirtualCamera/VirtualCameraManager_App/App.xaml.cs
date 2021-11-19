// Copyright (C) Microsoft Corporation. All rights reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Windows.ApplicationModel;
using Windows.ApplicationModel.Activation;
using Windows.ApplicationModel.AppService;
using Windows.ApplicationModel.Background;
using Windows.Foundation.Metadata;
using Windows.Storage;
using Windows.UI.Core;
using Windows.UI.Popups;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Navigation;

namespace VirtualCameraManager_App
{
    /// <summary>
    /// Provides application-specific behavior to supplement the default Application class.
    /// </summary>
    sealed partial class App : Application
    {
        /// <summary>
        /// Internal class to cache the parameters with which a previous virtual camera was created in the app upon suspending.
        /// This allows then the app to take control of previous virtual cameras it knows how it created upon launching.
        /// </summary>
        internal class VCamInfo
        {
            public string FriendlyName;
            public int VCamType;
            public string SymLink;
            public string WrappedCameraSymLink;
        }

        // members to handle known virtual cameras
        internal static List<VCamInfo> m_virtualCameraInfoList = new List<VCamInfo>();
        internal SemaphoreSlim m_lock = new SemaphoreSlim(1);
        private const string CONFIG_FILE_NAME = "config.txt";

        public static BackgroundTaskDeferral AppServiceDeferral = null;
        public static AppServiceConnection Connection = null;
        public static bool IsForeground = false;

        /// <summary>
        /// Initializes the singleton application object.  This is the first line of authored code
        /// executed, and as such is the logical equivalent of main() or WinMain().
        /// </summary>
        public App()
        {
            this.InitializeComponent();
            this.Suspending += OnSuspending;
        }

        protected override void OnBackgroundActivated(BackgroundActivatedEventArgs args)
        {
            // connection established from the fulltrust process
            base.OnBackgroundActivated(args);
            if (args.TaskInstance.TriggerDetails is AppServiceTriggerDetails details)
            {
                AppServiceDeferral = args.TaskInstance.GetDeferral();
                args.TaskInstance.Canceled += OnTaskCanceled;
                Connection = details.AppServiceConnection;
                Connection.RequestReceived += OnRequestReceived;
            }
        }

        internal static async Task LaunchSysTrayAsync()
        {
            if (ApiInformation.IsApiContractPresent("Windows.ApplicationModel.FullTrustAppContract", 1, 0))
            {
                await FullTrustProcessLauncher.LaunchFullTrustProcessForCurrentAppAsync();
            }
        }

        private async void OnRequestReceived(AppServiceConnection sender, AppServiceRequestReceivedEventArgs args)
        {
            if (args.Request.Message.ContainsKey("content"))
            {
                object message = null;
                args.Request.Message.TryGetValue("content", out message);
                if (App.IsForeground)
                {
                    await Windows.ApplicationModel.Core.CoreApplication.MainView.Dispatcher.RunAsync(
                        CoreDispatcherPriority.Normal, 
                        new DispatchedHandler(async () =>
                    {
                        MessageDialog dialog = new MessageDialog(message.ToString());
                        await dialog.ShowAsync();
                    }));
                }
            }

            if (args.Request.Message.ContainsKey("exit"))
            {
                App.Current.Exit();
            }
        }
        private void OnTaskCanceled(IBackgroundTaskInstance sender, BackgroundTaskCancellationReason reason)
        {
            if (AppServiceDeferral != null)
            {
                AppServiceDeferral.Complete();
            }
        }

        protected override async void OnActivated(IActivatedEventArgs args)
        {
            Debug.WriteLine($"#################### OnActivated .. kind={args.Kind}\n");
            string protocolArg = "";

            // Check if already have a window created
            Frame rootFrame = Window.Current.Content as Frame;
            bool isNeedingANewFrame = rootFrame == null;

            // Handle request to close app or window
            if (args.Kind == ActivationKind.Protocol)
            {
                ProtocolActivatedEventArgs eventArgs = args as ProtocolActivatedEventArgs;
                Debug.WriteLine($"#################### ProtocolActivatedEventArgs ..\n");
                protocolArg = eventArgs.Uri.AbsolutePath;

                // we exit the app from systray
                if (protocolArg == "exit")
                {
                    Application.Current.Exit();
                    return;
                }
            }
            // if we are asked to start at startup, exit and defer to systray background process behavior
            else if (args.Kind == ActivationKind.StartupTask)
            {
                await LaunchSysTrayAsync();
                Application.Current.Exit();
                return;
            }

            // Do not repeat app initialization when the Window already has content,  
            // just ensure that the window is active  
            if (isNeedingANewFrame)
            {
                // Create a Frame to act as the navigation context and navigate to the first page  
                rootFrame = new Frame();

                rootFrame.NavigationFailed += OnNavigationFailed;
                // Place the frame in the current Window  
                Window.Current.Content = rootFrame;

                // Load virtual camera configurations (if any)
                await LoadConfigFileAsync();

                // If we are asked to open the main control app
                if (protocolArg == "")
                {
                    Windows.UI.ViewManagement.ApplicationView.PreferredLaunchViewSize = new Windows.Foundation.Size(400, 600);
                    Windows.UI.ViewManagement.ApplicationView.GetForCurrentView().SetPreferredMinSize(new Windows.Foundation.Size(400, 600));
                    Windows.UI.ViewManagement.ApplicationView.PreferredLaunchWindowingMode = Windows.UI.ViewManagement.ApplicationViewWindowingMode.PreferredLaunchViewSize;
                    Window.Current.Activate();
                    await Windows.UI.ViewManagement.ApplicationView.GetForCurrentView().TryEnterViewModeAsync(Windows.UI.ViewManagement.ApplicationViewMode.Default);
                    Windows.UI.ViewManagement.ApplicationView.GetForCurrentView().TryResizeView(new Windows.Foundation.Size(400, 600));
                    rootFrame.Navigate(typeof(MainPage), args.Kind);
                }
            }
            else
            {
                Window.Current.Activate();
            }
            base.OnActivated(args);
        }
        /// Invoked when the application is launched normally by the end user.  Other entry points
        /// will be used such as when the application is launched to open a specific file.
        /// </summary>
        /// <param name="e">Details about the launch request and process.</param>
        protected override async void OnLaunched(LaunchActivatedEventArgs e)
        {
            // Load virtual camera configurations (if any)
            await LoadConfigFileAsync();

            var t = LaunchSysTrayAsync(); //fire-forget
            Frame rootFrame = Window.Current.Content as Frame;

            // Do not repeat app initialization when the Window already has content,
            // just ensure that the window is active
            if (rootFrame == null)
            {
                // Create a Frame to act as the navigation context and navigate to the first page
                rootFrame = new Frame();

                rootFrame.NavigationFailed += OnNavigationFailed;

                if (e.PreviousExecutionState == ApplicationExecutionState.Terminated)
                {
                    //TODO: Load state from previously suspended application
                }

                // Place the frame in the current Window
                Window.Current.Content = rootFrame;
            }
            Windows.UI.ViewManagement.ApplicationView.PreferredLaunchViewSize = new Windows.Foundation.Size(400, 400);
            Windows.UI.ViewManagement.ApplicationView.GetForCurrentView().SetPreferredMinSize(new Windows.Foundation.Size(400, 400));
            Windows.UI.ViewManagement.ApplicationView.PreferredLaunchWindowingMode = Windows.UI.ViewManagement.ApplicationViewWindowingMode.PreferredLaunchViewSize;

            if (e.PrelaunchActivated == false)
            {
                if (rootFrame.Content == null)
                {
                    // When the navigation stack isn't restored navigate to the first page,
                    // configuring the new page by passing required information as a navigation
                    // parameter
                    rootFrame.Navigate(typeof(MainPage), e.Arguments);
                }
                // Ensure the current window is active
                Window.Current.Activate();
            }
        }

        /// <summary>
        /// Invoked when Navigation to a certain page fails
        /// </summary>
        /// <param name="sender">The Frame which failed navigation</param>
        /// <param name="e">Details about the navigation failure</param>
        void OnNavigationFailed(object sender, NavigationFailedEventArgs e)
        {
            throw new Exception("Failed to load Page " + e.SourcePageType.FullName);
        }

        /// <summary>
        /// Invoked when application execution is being suspended.  Application state is saved
        /// without knowing whether the application will be terminated or resumed with the contents
        /// of memory still intact.
        /// </summary>
        /// <param name="sender">The source of the suspend request.</param>
        /// <param name="e">Details about the suspend request.</param>
        private async void OnSuspending(object sender, SuspendingEventArgs e)
        {
            var deferral = e.SuspendingOperation.GetDeferral();

            // Save virtual camera configurations (if any)
            await WriteToConfigFileAsync();

            deferral.Complete();
        }

        /// <summary>
        /// Method to search and load if found a local config file containing 
        /// the information about previously registered virtual cameras with this app.
        /// </summary>
        /// <returns></returns>
        private async Task LoadConfigFileAsync()
        {
            m_lock.Wait();
            try
            {
                var item = await ApplicationData.Current.LocalFolder.TryGetItemAsync(CONFIG_FILE_NAME);
                if (item != null)
                {
                    var configFile = item as StorageFile;
                    var configurations = await Windows.Storage.FileIO.ReadLinesAsync(configFile);
                    foreach (var line in configurations)
                    {
                        var configItems = line.Split("|");
                        if (configItems.Count() == 4)
                        {
                            m_virtualCameraInfoList.Add(new VCamInfo()
                            {
                                FriendlyName = configItems[0],
                                VCamType = Int32.Parse(configItems[1]),
                                SymLink = configItems[2],
                                WrappedCameraSymLink = configItems[3]
                            });
                        }
                    }
                }
            }
            finally
            {
                m_lock.Release();
            }
        }

       
        /// <summary>
        /// Method to write to a config files the set of Virtual Cameras created with the app
        /// </summary>
        /// <returns></returns>
        private async Task WriteToConfigFileAsync()
        {
            m_lock.Wait();
            try
            {
                var item = await ApplicationData.Current.LocalFolder.TryGetItemAsync(CONFIG_FILE_NAME);
                StorageFile configFile = null;
                if (item != null)
                {
                    configFile = item as StorageFile;

                }
                else
                {
                    configFile = await ApplicationData.Current.LocalFolder.CreateFileAsync(CONFIG_FILE_NAME);
                }

                List<string> vCamInfoToWrite = new List<string>();
                foreach (var vcam in m_virtualCameraInfoList)
                {
                    {
                        vCamInfoToWrite.Add(vcam.FriendlyName
                            + "|" + (int)vcam.VCamType
                            + "|" + vcam.SymLink
                            + "|" + vcam.WrappedCameraSymLink);
                    }
                }

                await FileIO.WriteLinesAsync(configFile, vCamInfoToWrite);
            }
            finally
            {
                m_lock.Release();
            }
        }
    }
}
