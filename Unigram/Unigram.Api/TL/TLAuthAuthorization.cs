// <auto-generated/>
using System;

namespace Telegram.Api.TL
{
	public partial class TLAuthAuthorization : TLObject 
	{
		public TLUserBase User { get; set; }

		public TLAuthAuthorization() { }
		public TLAuthAuthorization(TLBinaryReader from, TLType type = TLType.AuthAuthorization)
		{
			Read(from, type);
		}

		public override TLType TypeId { get { return TLType.AuthAuthorization; } }

		public override void Read(TLBinaryReader from, TLType type = TLType.AuthAuthorization)
		{
			User = TLFactory.Read<TLUserBase>(from);
		}

		public override void Write(TLBinaryWriter to)
		{
			to.Write(0xFF036AF1);
			to.WriteObject(User);
		}
	}
}