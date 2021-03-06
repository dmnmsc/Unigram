// <auto-generated/>
using System;

namespace Telegram.Api.TL.Methods.Messages
{
	/// <summary>
	/// RCP method messages.deleteMessages.
	/// Returns <see cref="Telegram.Api.TL.TLMessagesAffectedMessages"/>
	/// </summary>
	public partial class TLMessagesDeleteMessages : TLObject
	{
		[Flags]
		public enum Flag : Int32
		{
			Revoke = (1 << 0),
		}

		public bool IsRevoke { get { return Flags.HasFlag(Flag.Revoke); } set { Flags = value ? (Flags | Flag.Revoke) : (Flags & ~Flag.Revoke); } }

		public Flag Flags { get; set; }
		public TLVector<Int32> Id { get; set; }

		public TLMessagesDeleteMessages() { }
		public TLMessagesDeleteMessages(TLBinaryReader from)
		{
			Read(from);
		}

		public override TLType TypeId { get { return TLType.MessagesDeleteMessages; } }

		public override void Read(TLBinaryReader from)
		{
			Flags = (Flag)from.ReadInt32();
			Id = TLFactory.Read<TLVector<Int32>>(from);
		}

		public override void Write(TLBinaryWriter to)
		{
			to.Write(0xE58E95D2);
			to.Write((Int32)Flags);
			to.WriteObject(Id);
		}
	}
}