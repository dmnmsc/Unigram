// <auto-generated/>
using System;

namespace Telegram.Api.TL.Methods.Messages
{
	/// <summary>
	/// RCP method messages.forwardMessages
	/// </summary>
	public partial class TLMessagesForwardMessages : TLObject
	{
		[Flags]
		public enum Flag : int
		{
			Silent = (1 << 5),
			Background = (1 << 6),
		}

		public bool IsSilent { get { return Flags.HasFlag(Flag.Silent); } set { Flags = value ? (Flags | Flag.Silent) : (Flags & ~Flag.Silent); } }
		public bool IsBackground { get { return Flags.HasFlag(Flag.Background); } set { Flags = value ? (Flags | Flag.Background) : (Flags & ~Flag.Background); } }

		public Flag Flags { get; set; }
		public TLInputPeerBase FromPeer { get; set; }
		public TLVector<Int32> Id { get; set; }
		public TLVector<Int64> RandomId { get; set; }
		public TLInputPeerBase ToPeer { get; set; }

		public TLMessagesForwardMessages() { }
		public TLMessagesForwardMessages(TLBinaryReader from, TLType type = TLType.MessagesForwardMessages)
		{
			Read(from, type);
		}

		public override TLType TypeId { get { return TLType.MessagesForwardMessages; } }

		public override void Read(TLBinaryReader from, TLType type = TLType.MessagesForwardMessages)
		{
			Flags = (Flag)from.ReadInt32();
			FromPeer = TLFactory.Read<TLInputPeerBase>(from);
			Id = TLFactory.Read<TLVector<Int32>>(from);
			RandomId = TLFactory.Read<TLVector<Int64>>(from);
			ToPeer = TLFactory.Read<TLInputPeerBase>(from);
		}

		public override void Write(TLBinaryWriter to)
		{
			to.Write(0x708E0195);
			to.Write((Int32)Flags);
			to.WriteObject(FromPeer);
			to.WriteObject(Id);
			to.WriteObject(RandomId);
			to.WriteObject(ToPeer);
		}
	}
}