using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Telegram.Api.TL;
using Unigram.Views;
using Unigram.ViewModels;
using Unigram.ViewModels.Chats;
using Unigram.Views.Users;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;

namespace Unigram.Views.Chats
{
    public sealed partial class ChatDetailsPage : Page
    {
        public ChatDetailsViewModel ViewModel => DataContext as ChatDetailsViewModel;

        public ChatDetailsPage()
        {
            InitializeComponent();
            DataContext = UnigramContainer.Current.ResolveType<ChatDetailsViewModel>();
        }

        private void Photo_Click(object sender, RoutedEventArgs e)
        {

        }

        private void EditPhoto_Click(object sender, RoutedEventArgs e)
        {

        }
    }
}
